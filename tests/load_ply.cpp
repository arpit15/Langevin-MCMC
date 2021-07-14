#include "parseply.h"

using namespace std;
int main() {
    string meshFn = "../data/unit_plane.ply";

    bool isMoving = false, 
        flipNormals = false, 
        faceNormals = false;

    Matrix4x4 toWorld0 = Matrix4x4::Identity(),
        toWorld1 = Matrix4x4::Identity();
    std::shared_ptr<TriMeshData> mesh = ParsePly(
            meshFn,
            toWorld0,
            toWorld1,
            isMoving, 
            flipNormals,
            faceNormals
    );

    // print info
    cout << "Mesh contains " << mesh->position0.size() << " vertices "
        << mesh->indices.size() << " faces" << endl;
    cout << "Vertices Info " << endl;
    for (size_t i = 0; i < mesh->position0.size(); ++i)
    {
        Vector3 &vert = mesh->position0.at(i);
        cout << i << " " << vert[0] << " " << vert[1] << " " << vert[2] << endl;
    }
    cout << "-------------" << endl;

    cout << "Faces Info " << endl;
    for (size_t i = 0; i < mesh->indices.size(); ++i)
    {
        TriIndex &idx = mesh->indices.at(i);
        cout << i << " " << idx.index[0] << " " << idx.index[1] << " " << idx.index[2] << endl;
    }
    cout << "-------------" << endl;
    
    return 0;
}