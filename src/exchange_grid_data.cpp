// Copyright 2020, the Aether Development Team (see doc/dev_team.md for members)
// Full license can be found in License.md

#include "aether.h"
#include <vector>
#include <mpi.h>

std::vector<char> pack_grid(Grid &grid) {
    std::vector<char> data;
    
    return data;
}

Grid unpack_grid(std::vector<char> &data, Grid &grid) {;
    return grid;
}

bool exchange_grid_data(Grid &grid1, Grid &grid2, const MPI_Comm &comm) {
    MPI_Barrier(comm);
    
    std::vector<char> data1 = pack_grid(grid1);
    std::vector<char> data2 = pack_grid(grid2);

    int rank1;
    MPI_Comm_rank(comm, &rank1);

    int rank2;
    MPI_Comm_rank(comm, &rank2);

    std::vector<char> recv_buffer1(data1.size());
    std::vector<char> recv_buffer2(data2.size());

    MPI_Request requests[4];
    MPI_Isend(data1.data(), data1.size(), MPI_BYTE, rank2, 0, comm, &requests[0]);
    MPI_Isend(data2.data(), data2.size(), MPI_BYTE, rank1, 0, comm, &requests[1]);
    MPI_Irecv(recv_buffer1.data(), recv_buffer1.size(), MPI_BYTE, rank2, 0, comm, &requests[2]);
    MPI_Irecv(recv_buffer2.data(), recv_buffer2.size(), MPI_BYTE, rank1, 0, comm, &requests[3]);

    MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);

    grid1 = unpack_grid(recv_buffer2, grid1);
    grid2 = unpack_grid(recv_buffer1, grid2);
}