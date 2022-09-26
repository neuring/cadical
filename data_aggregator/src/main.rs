use std::{collections::HashMap, path::{PathBuf, Path}, sync::{Arc, Mutex}, process::Stdio, time::Duration, str::FromStr, sync::MutexGuard, fmt};

use anyhow::Context;
use clap::Parser;
use sysinfo::{System, SystemExt, Pid, ProcessExt};
use tokio::{net::{UnixStream, UnixListener}, io::{AsyncReadExt, BufReader, AsyncRead}, select, sync::{mpsc, oneshot, Semaphore, OnceCell}, process::Command, fs::read_to_string};

static SYSINFO: OnceCell<Mutex<System>> = OnceCell::const_new();

async fn init_system() -> Mutex<System> {
    let sys = System::new_all();
    Mutex::new(sys)
}

#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
struct Config {
    /// Path of the socket to be used for IPC between the sat solvers and this server.
    #[clap(long, default_value="/tmp/lbd_exchange.sock")]
    pipe_path: String,

    /// Path of the SAT solver executable.
    #[clap(short, long)]
    cadical_path: PathBuf,

    /// Dimacs file of input problem.
    #[clap(short, long)]
    file: String,

    /// File containing newline separated list of flags to be used for each individual solver execution.
    #[clap(short, long)]
    instance_configs: PathBuf,

    /// How many instances of sat solvers to run in parallel.
    #[clap(short, long, default_value="2")]
    parallel: u32,

    /// How much memory can the combined cadical processes use max. (in mb)
    #[clap(short, long, default_value_t)]
    max_mem_usage: Memory,

    solver_flags: Vec<String>,
}

#[derive(Debug)]
struct Memory {
    kb: u64,
}

impl fmt::Display for Memory {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mb : f32 = self.kb as f32 / 1024.;
        write!(f, "{:.2}", mb)
    }
}

impl FromStr for Memory {
    type Err = <f32 as FromStr>::Err;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let val = f32::from_str(s.trim())?;
        Ok(Self { kb: (val * 1024.) as u64 })
    }
}

impl Default for Memory {
    fn default() -> Self {
        let memory = async {
            let sys: &System = &*SYSINFO.get_or_init(init_system).await.lock().unwrap();
            sys.total_memory()
        };

        let rt = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build().unwrap();

        // Call the asynchronous connect method using the runtime.
        let memory = rt.block_on(memory);

        Self { kb: memory }
    }
}

#[derive(Debug)]
struct ClauseData {
    literals: Box<[i32]>,
    lbd: i32,
}

async fn read_clause(stream: &mut (impl AsyncRead + Unpin)) -> anyhow::Result<ClauseData> {
    let mut literals = Vec::new();
    loop {
        let lit = stream.read_i32_le().await.context("Reading literal failed")?;

        if lit == 0 {
            // End of clause reached.
            break;
        } else {
            literals.push(lit);
        }
    }

    let lbd = stream.read_i32_le().await.context("Reading lbd value failed")?;

    literals.sort_by_key(|lit| lit.abs());
    Ok(ClauseData{
        literals: literals.into_boxed_slice(), lbd
    })
}

async fn incoming_data_handler(stream: UnixStream, sender: mpsc::Sender<ClauseData>) -> anyhow::Result<()> {
    let mut stream = BufReader::new(stream);

    loop {
        let clause_data = read_clause(&mut stream).await.context("Reading clause failed")?;
        //println!("Received clause {:?}", clause_data);
        sender.send(clause_data).await.context("Sending clause data failed")?;
    }
}

struct ClauseStats {
    count: u32,
    mean: f32,
    m2: f32,
}

impl ClauseStats {
    fn update(&mut self, new_value: i32) {
        let new_value = new_value as f32;

        self.count += 1;
        let delta = new_value - self.mean;
        self.mean += delta / self.count as f32;
        let delta2 = new_value - self.mean;
        self.m2 += delta * delta2;
    }

    fn final_count(&self) -> u32 {
        self.count
    }
    fn final_mean(&self) -> f32 {
        self.mean
    }
    fn final_variance(&self) -> f32 {
        if self.count < 2 {
            0.0
        } else {
            self.m2 / ((self.count - 1) as f32)
        }
    }
}

type ClauseMap = HashMap<Box<[i32]>, ClauseStats>;

fn update_data_map(aggregate: &mut ClauseMap, new_data: &ClauseData) {
    if let Some(current) = aggregate.get_mut(&new_data.literals) {
        current.update(new_data.lbd);
    } else {
        aggregate.insert(new_data.literals.clone(), ClauseStats { count: 1, mean: new_data.lbd as f32, m2: 0. });
    }
}

async fn data_aggregation_handler(mut receiver: mpsc::Receiver<ClauseData>, mut finish: oneshot::Receiver<()>) -> anyhow::Result<ClauseMap> {
    let mut data = ClauseMap::new();

    loop {
        select! {
            received = receiver.recv() => {
                let clause_data = received.expect("Data channel unexpectedly closed.");
                update_data_map(&mut data, &clause_data);
            }
            _ = tokio::signal::ctrl_c() =>  {
                eprintln!("Termination signal received");
                return Ok(data);
            }
            _ = &mut finish => {
                eprintln!("Finished all runs");
                return Ok(data);
            }
        }
    }

}

async fn spawn_cadical(config: Arc<Config>, instance_conf: Vec<String>) -> anyhow::Result<()> {
    let mut cmd = Command::new(&config.cadical_path);
    cmd 
        .arg(&config.file)
        .args(&config.solver_flags)
        .args(&instance_conf)
        .arg("--lbd-socket-path")
        .arg(&config.pipe_path)
        .stdout(Stdio::null());

    let mut child_handle = cmd.spawn().context("Couldn't spawn cadical child process")?;
    let child_pid = Pid::from(child_handle.id().context("Child doesn't have pid")? as i32);

    loop {
        select! {
            _ = child_handle.wait() => {
                // Child process has finished
                break
            }
            _ = tokio::time::sleep(Duration::from_secs(1)) => {
                // Check if child process consumes too much memory, if so kill it
                let memory_usage = {
                    let mut sysinfo: MutexGuard<System> = SYSINFO.get_or_init(init_system).await.lock().unwrap();
                    assert!(sysinfo.refresh_process(child_pid));
                    sysinfo.process(child_pid).unwrap().memory()
                };

                if memory_usage > config.max_mem_usage.kb / config.parallel as u64 {
                    child_handle.kill().await.context("Couldn't kill child that uses too much memory")?;
                    eprintln!("Cadical process used too much memory");
                }
            }
        }
    }
    // Cadical returns 10 on success
    //if !exit.success() {
    //    eprintln!("Exit code of cadical ({number}) is error.");
    //}

    Ok(())
}

async fn cadical_spawner(config: Arc<Config>, finish: oneshot::Sender<()>, instance_configs: Vec<Vec<String>>) -> anyhow::Result<()> {

    let sem = Arc::new(Semaphore::new(config.parallel as usize));

    let mut handles = Vec::new();

    let instances = instance_configs.len();

    for (i, instance_config) in instance_configs.into_iter().enumerate() {
        eprintln!("progress {}/{}", i, instances);
        let permit = sem.clone().acquire_owned().await.context("Error on semaphore acquire.")?;
        let config_clone = config.clone();

        let handle = tokio::spawn(async move {
            let _p = permit;

            spawn_cadical(config_clone, instance_config).await
        });

        handles.push(handle);
    }

    for handle in handles {
        handle.await?.context("Cadical invocation failed")?;
    }

    finish.send(()).unwrap();

    Ok(())
}

async fn read_instance_config(path: &Path) -> anyhow::Result<Vec<Vec<String>>> {
    let configs = read_to_string(path).await.with_context(|| format!("Failed to read instance configuration from path '{}'", path.to_string_lossy()))?;

    Ok(
        configs.split_terminator('\n')
               .map(|s| s.split_whitespace()
                         .map(|s| s.to_owned())
                         .collect::<Vec<_>>())
               .collect()
    )
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let config = Arc::new(Config::parse());

    let (sender, receiver) = tokio::sync::mpsc::channel(10000);


    let listener = UnixListener::bind(&config.pipe_path).context("Failed to create pipe.")?;

    tokio::spawn(async move {
        loop {
            let cloned_sender = sender.clone();
            match listener.accept().await {
                Ok((stream, _)) =>  {
                    tokio::spawn(incoming_data_handler(stream, cloned_sender));
                }
                Err(err) => eprintln!("FAiled to accept connection {:?}", err)
            }
        }
    });

    let (finish_sender, finish_receiver) = tokio::sync::oneshot::channel();

    let aggregator = data_aggregation_handler(receiver, finish_receiver);

    let instance_configs = read_instance_config(&config.instance_configs).await?;
    let _spawner_handle = tokio::spawn(cadical_spawner(config.clone(), finish_sender, instance_configs));

    let result = aggregator.await;
    let result = result.context("Data aggregation failed")?;
    eprintln!("unique clauses: {}", result.len());

    for (clause, stats) in result.iter() {
        if stats.final_variance() == 0. {
            continue;
        }
        println!("{},{},{},{}", stats.final_count(), stats.final_mean(), stats.final_variance(), clause.len())
    }

    tokio::fs::remove_file(&config.pipe_path).await.context("Couldn't remove socket file")
}
