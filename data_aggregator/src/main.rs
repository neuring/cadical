use std::{collections::HashMap, path::PathBuf, sync::Arc, process::Stdio};

use anyhow::Context;
use clap::Parser;
use tokio::{net::{UnixStream, UnixListener}, io::{AsyncReadExt, BufReader, AsyncRead}, select, sync::{mpsc, oneshot, Semaphore}, process::Command};

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

    /// Number of times to run the sat solver with different seeds.
    #[clap(short, long, default_value="2")]
    iterations: u32,

    /// How many instances of sat solvers to run in parallel.
    #[clap(short, long, default_value="2")]
    parallel: u32,

    solver_flags: Vec<String>,
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
                println!("Termination signal received");
                return Ok(data);
            }
            _ = &mut finish => {
                println!("Finished all runs");
                return Ok(data);
            }
        }
    }

}

async fn spawn_cadical(config: Arc<Config>, number: u32) -> anyhow::Result<()> {
    let mut cmd = Command::new(&config.cadical_path);
    cmd 
        .arg(&config.file)
        .arg("--seed")
        .arg(&number.to_string())
        .arg("--lbd-socket-path")
        .arg(&config.pipe_path)
        .args(&config.solver_flags)
        .stdout(Stdio::null());

    cmd.status().await.context("Failed to execute cadical")?;

    // Cadical returns 10 on success
    //if !exit.success() {
    //    eprintln!("Exit code of cadical ({number}) is error.");
    //}

    Ok(())
}

async fn cadical_spawner(config: Arc<Config>, finish: oneshot::Sender<()>) -> anyhow::Result<()> {

    let sem = Arc::new(Semaphore::new(config.parallel as usize));

    let mut handles = Vec::new();

    for i in 1..=config.iterations {
        println!("progress {}/{}", i, config.iterations);
        let permit = sem.clone().acquire_owned().await.context("Error on semaphore acquire.")?;
        let config_clone = config.clone();

        let handle = tokio::spawn(async move {
            let _p = permit;

            spawn_cadical(config_clone, i).await
        });

        handles.push(handle);
    }

    for handle in handles {
        handle.await?.context("Cadical invocation failed")?;
    }

    finish.send(()).unwrap();

    Ok(())
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

    let _spawner_handle = tokio::spawn(cadical_spawner(config.clone(), finish_sender));

    let result = aggregator.await;
    let result = result.context("Data aggregation failed")?;
    println!("unique clauses: {}", result.len());

    for (clause, stats) in result.iter() {
        if stats.final_variance() == 0. {
            continue;
        }
        println!("count = {}, mean = {}, variance = {}, ({:<?})", stats.final_count(), stats.final_mean(), stats.final_variance(), clause)
    }

    tokio::fs::remove_file(&config.pipe_path).await.context("Couldn't remove socket file")
}
