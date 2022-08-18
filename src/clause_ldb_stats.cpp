
#include "internal.hpp"

namespace CaDiCaL {

// Implementation of welfords online algorithm, copied from wikipedia:
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
void LBDAggregate::update(float new_value) {
    this->count += 1;
    float delta = new_value - this->mean;
    this->mean += delta / count;
    float delta2 = new_value - this->mean;
    this->m2 += delta * delta2;
}

float LBDAggregate::final_mean() {
    return this->mean;
}

float LBDAggregate::final_variance() {
    if (this->count == 1) {
        return 0.0;
    } else {
        return this->m2 / (this->count - 1);
    }
}

int LBDAggregate::final_count() {
    return this->count;
}

void LBDStats::update(std::vector<int> const& clause, int lbd_value) {
    std::vector<int> cls = clause;
    std::sort(cls.begin(), cls.end());
    auto it = this->data.find(cls);

    if (it != this->data.end()) {
        // in map
        it->second.update(lbd_value);
    } else {
        // Not in map
        this->data[cls] = {1, (float) lbd_value, 0};
    }
}

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void Internal::send_clause_to_aggregator(std::vector<int>& clause, int lbd) {
    if (this->lbd_socket_path == nullptr) return;

    size_t size_in_bytes = clause.size() * sizeof(int);
    size_t bytes_written = write(this->lbd_socket_fd, &clause[0], size_in_bytes);

    if (bytes_written != size_in_bytes) {
      if (bytes_written > 0) fprintf(stderr,"partial write\n");
      else {
        perror("write error\n");
        exit(-1);
      }
    }

    int remaining_data[2] = {0, lbd};

    size_in_bytes = sizeof(remaining_data);
    bytes_written = write(this->lbd_socket_fd, remaining_data, size_in_bytes);
    if (bytes_written != size_in_bytes) {
      if (bytes_written > 0) fprintf(stderr,"partial write\n");
      else {
        perror("write error\n");
        exit(-1);
      }
    }
}

void Internal::init_lbd_aggregator() {
    printf("Initializing lbd socket\n");
    struct sockaddr_un addr;

    if (this->lbd_socket_path == nullptr) return;

    if ( (this->lbd_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error\n");
        exit(-1);
    }

    printf("Socket fd is %d\n", this->lbd_socket_fd);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, this->lbd_socket_path, sizeof(addr.sun_path)-1);

    if (connect(this->lbd_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error\n");
        exit(-1);
    }
}

}