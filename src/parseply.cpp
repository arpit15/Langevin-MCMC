#include "parseply.h"
#include "transform.h"
#include "utils.h"

#include <map>
#include <fstream>
#include <regex>
#include <string>
#include <stdexcept>

enum ByteOrder {
        LittleEndian,
        BigEndian,
        HostByteOrder
    };

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](int c) {return !std::isspace(c);}));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](int c) {return !std::isspace(c);}).base(),
        s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

ByteOrder getMachineEndianness() {
    unsigned char i=1;
    char *c = (char *)&i;
    if (*c)
        return ByteOrder::LittleEndian;
    else
        return ByteOrder::BigEndian;
}

std::shared_ptr<TriMeshData> ParsePly(const std::string &filename,
                                      const Matrix4x4 &toWorld0,
                                      const Matrix4x4 &toWorld1,
                                      bool isMoving,
                                      bool flipNormals,
                                      bool faceNormals) {
    
    std::shared_ptr<TriMeshData> data = std::make_shared<TriMeshData>();
    data->isMoving = isMoving;

    std::ifstream ifs(filename.c_str(), std::ifstream::in);

    if (!ifs.is_open())
        throw std::runtime_error("Unable to open the PLY file: " + filename);
    else
        std::cout << "Parsing " << filename << std::endl;

    bool ply_tag_seen = false,
     ascii = false,
     header_processsed = false,
     contain_vert_normal = false,
     contain_uv = false;

    ByteOrder byte_order = ByteOrder::HostByteOrder;

    unsigned int stream_pos;
    // parse header
    while (ifs.good()) {
        std::string line;
        std::getline(ifs, line);
        line = trim(line);
        if (line.size() == 0)
            continue;

        std::stringstream ss(line);
        std::string token;
        ss >> token;

        // std::cout << token << std::endl;

        if ( token ==  "comment")  // comment
            continue;
        else if (token == "ply")
            ply_tag_seen = true;
        else if( token == "format") {
            if(!ply_tag_seen)
                throw std::runtime_error("invalid PLY header: \"format\" before \"ply\" tag");
            if(header_processsed)
                throw std::runtime_error("invalid PLY header: duplicate \"format\" tag");
            
            if (!(ss >> token))
                throw std::runtime_error("invalid PLY header: missing token after \"format\"");

            if ( token == "ascii")
                ascii = true;
            else if (token == "binary_little_endian")
                byte_order = ByteOrder::LittleEndian;
            else if (token == "binary_big_endian")
                byte_order = ByteOrder::BigEndian;
            else
                throw std::runtime_error("invalid PLY header: invalid token after \"format\"");

            if (!(ss >> token))
                throw std::runtime_error("invalid PLY header: missing version number after \"format\"");
            if (token != "1.0")
                throw std::runtime_error("PLY file has unknown version number " + token);
            if (ss >> token)
                throw std::runtime_error("invalid PLY header: excess tokens after \"format\"");
            header_processsed = true;
        } else if ( token == "element" ) {
            ss >> token;
            if ( token == "vertex" ) {
                int numVert;
                ss >> numVert;
                // std::cout << "Num verts : " << numVert << std::endl; 
                data->position0.resize(numVert);
                data->position1.resize(numVert);
                // posPool.resize(numVert);
            } else if ( token == "face" ) {
                int numFaces;
                ss >> numFaces;
                // std::cout << "Num faces : " << numFaces << std::endl;
                // posPool.resize(numVert);
                data->indices.resize(numFaces);
            } 

        } else if ( token == "property" ) {
            ss >> token;  // type
            ss >> token;
            // std::cout << "Parsing property : " << token << std::endl;
            if ( token == "nx") {
                contain_vert_normal = true;
                data->normal0.resize(data->position0.size());
                data->normal1.resize(data->position1.size());
            } else if ( token == "u" || token == "s") {
                contain_uv = true;
                data->st.resize(data->position0.size());
            }

        } else if ( token == "end_header" ) {
            stream_pos = ifs.tellg();
            break;
        }
    }

    ifs.close();


    if(ascii)
        ifs = std::ifstream(filename.c_str(), std::ifstream::in);
    else    
        ifs = std::ifstream(filename.c_str(), std::ifstream::in | std::ifstream::binary);
    
    ifs.seekg(stream_pos);

    // std::cout << "Ascii format : " << ascii << std::endl;
    if ( getMachineEndianness() != byte_order)
        std::cout << "Problem detected! machine endianness : " << getMachineEndianness() << ", file endianness : " << byte_order << std::endl;

    // std::cout << "contain vert normals : " << contain_vert_normal << ", contain uv : " << contain_uv << std::endl;
    // parse the vert data
    int numVertParsed = 0;
    while (ifs.good()) {


        // get vertex data
        if (numVertParsed < data->position0.size()) {
            std::string line;
            std::stringstream ss;
            // std::string token;
            if (ascii) {
                // parse per line
                std::getline(ifs, line);
                line = trim(line);
                
                if (line.size() == 0)
                    continue;            
                // ss << line;
                ss.str(line);
                // ss >> token;
            }

            Float x, y, z;
            if (ascii)
                ss >> x >> y >> z;
            else{
                                
                ifs.read(reinterpret_cast<char*>(&x), sizeof(Float));
                ifs.read(reinterpret_cast<char*>(&y), sizeof(Float));
                ifs.read(reinterpret_cast<char*>(&z), sizeof(Float));
            }
                

            Vector3 vert(x,y,z);
            // std::cout << numVertParsed << " vert " << vert.transpose() << std::endl;
            data->position0[numVertParsed] = (XformPoint(toWorld0, vert));
            data->position1[numVertParsed] = (XformPoint(toWorld1, vert));

            // parse normals
            if (contain_vert_normal) {
                Float nx, ny, nz;
                if (ascii)
                    ss >> nx >> ny >> nz;
                else{
                                    
                    ifs.read(reinterpret_cast<char*>(&nx), sizeof(Float));
                    ifs.read(reinterpret_cast<char*>(&ny), sizeof(Float));
                    ifs.read(reinterpret_cast<char*>(&nz), sizeof(Float));
                }
                    

                Vector3 normal(nx,ny,nz);
                // std::cout << numVertParsed << " normal " << normal.transpose() << std::endl;
                data->normal0[numVertParsed] = (XformNormal(Matrix4x4(toWorld0.inverse()), normal));
                data->normal1[numVertParsed] = (XformNormal(Matrix4x4(toWorld1.inverse()), normal));
            }

            // parse uv
            if ( contain_uv ) {
                Float u, v;
                if (ascii)
                    ss >> u >> v;
                else{
                    ifs.read(reinterpret_cast<char*>(&u), sizeof(Float));
                    ifs.read(reinterpret_cast<char*>(&v), sizeof(Float));
                }
                    

                Vector2 uv(u, v);
                // std::cout << numVertParsed << " uv " << uv.transpose() << std::endl;
                data->st[numVertParsed] = uv;
            }

            ++numVertParsed;
        } else
            break;
        
    }

    int numFacesParsed = 0;
    while (ifs.good()) {


        // get face data
        if (numFacesParsed < data->indices.size()) {
            std::string line;
            std::stringstream ss;
            // std::string token;
            if (ascii) {
                
                std::getline(ifs, line);
                line = trim(line);
                if (line.size() == 0)
                    continue;      
                // ss << line; 
                ss.str(line);     
                // ss >> token;
            }

            // int numVertPerFace = 3;
            unsigned char numVertPerFace_char;
            if (ascii)
                ss >> numVertPerFace_char;
            else {    
                ifs.read(reinterpret_cast<char*>(&numVertPerFace_char), sizeof(numVertPerFace_char));
            }

            // std::cout << "readVal : " << numVertPerFace_char << std::endl;
            int numVertPerFace;
            if (ascii)
                numVertPerFace = numVertPerFace_char - '0';
            else
                numVertPerFace = numVertPerFace_char;
              
            if (numVertPerFace != 3)
                throw std::runtime_error(filename + ": Only support trimeshes!. Input mesh contains " + std::to_string(numVertPerFace) + " faces");
            
            int si0, si1, si2;

            if (ascii)
                ss >> si0 >> si1 >> si2;
            else {
                ifs.read(reinterpret_cast<char*>(&si0), sizeof(si0));
                ifs.read(reinterpret_cast<char*>(&si1), sizeof(si1));
                ifs.read(reinterpret_cast<char*>(&si2), sizeof(si2));
                // std::cout << "i0 : " << si0 << ", i1 : " << si1 << ", i2 : " << si2 << std::endl;
            }
                

            int v0id = (si0),
                v1id = (si1),
                v2id = (si2);

            // std::cout << "i0 : " << v0id << ", i1 : " << v1id << ", i2 : " << v2id << std::endl;
            data->indices[numFacesParsed] = (TriIndex(v0id, v1id, v2id));
            ++numFacesParsed;
        } else
            break;
    }

    if (data->normal0.size() == 0 || faceNormals) {
        ComputeNormal(data->position0, data->indices, data->normal0);
        ComputeNormal(data->position1, data->indices, data->normal1);
    }

    if (flipNormals) {
        for (int i = 0; i < data->normal0.size(); i++)
            data->normal0[i] = -data->normal0[i];
        for (int i = 0; i < data->normal1.size(); i++) 
            data->normal1[i] = -data->normal1[i];
    }

    // std::cout << "Loaded Mesh contains " << data->position0.size() << " vertices "
    //     << data->indices.size() << " faces" << std::endl;

    // std::cout << "Vert info" << std::endl;
    // // print mesh info
    // for (size_t i = 0; i < data->position0.size(); i++)
    // {
    //     std::cout << i << " " << data->position0.at(i).transpose() << std::endl;
    // }

    // std::cout << "=========" << std::endl;

    // if(data->normal0.size() > 0) {
    //     std::cout << "Normal info" << std::endl;
    //     for (size_t i = 0; i < data->normal0.size(); i++)
    //     {
    //         std::cout << i << " " << data->normal0.at(i).transpose() << std::endl;
    //     }
        
    //     std::cout << "================" << std::endl;
    // }

    // std::cout << "Face Info" << std::endl;

    // for (size_t i = 0; i < data->indices.size(); i++)
    // {
    //     std::cout << i << " " << data->indices.at(i) << std::endl;
    // }
    
    // std::cout << "===========" << std::endl;

    return data;
}