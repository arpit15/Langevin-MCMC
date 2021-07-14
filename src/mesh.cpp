#include "mesh.h"

std::ostream& operator<<(std::ostream& os, const TriIndex& triindex) {
    return os   << "id0:" << triindex.index[0] << ", "
                << "id1:" << triindex.index[1] << ", "
                << "id2:" << triindex.index[2] << std::endl;
}