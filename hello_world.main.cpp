#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <yaml-cpp/yaml.h>
#include "hello_imgui/hello_imgui.h"
#include "converter.h" // idk if this is the correct way to use header files :)
namespace fs = std::filesystem;

std::string onyx_path = "./dependencies/onyx-linux-x64.AppImage";
std::string superfreq_path = "./dependencies/superfreq";
std::string scene_tool_path = "./dependencies/scene_tool";
std::string objdir_path = "./dependencies/lipsync";
std::vector<std::string> command_log;

// this is my first time writing any c++ and i'm only writing it in c++ because i really need to learn it :3
// fuck data
int main(int , char *[]) {
#ifdef ASSETS_LOCATION
    HelloImGui::SetAssetsFolder(ASSETS_LOCATION);
#endif
    auto guiFunction = []() {
        ImGui::Text("Hello, ");                    // Display a simple label
        HelloImGui::ImageFromAsset("world.jpg");   // Display a static image
        if (ImGui::Button("Convert/Process Songs")) {
            // data would be so cool if it had threading
            std::thread song_thread(coolFunction);
            song_thread.detach();
        }
        if (ImGui::Button("Clear Log")) {
            command_log.clear();
        }
        if (ImGui::Button("Quit"))
            HelloImGui::GetRunnerParams()->appShallExit = true;
        if (command_log.size()) {
            ImGui::TextColored(ImVec4(1,1,0,1), "Converting Songs. This may take awhile.");
            ImGui::BeginChild("Scrolling");
            for (const auto & entry : command_log)
                ImGui::Text(entry.c_str());
            ImGui::EndChild();
        }
     };
    HelloImGui::Run(guiFunction, "LEGO Rock Band Wii Song Converter", true);
    return 0;
}

std::string tolower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); }
                  );
    return s;
}
std::string search_replace(std::string s, std::string search, std::string replace)
{
    search = "\\" + search;
    std::regex SearchRegex (search.c_str());
    s = std::regex_replace(s, SearchRegex, replace);
    return s;
}
void coolFunction() {
    command_log.clear();

    fs::remove_all("./temp");
    fs::create_directory("./temp");
    fs::create_directory("./temp/songs");
    
    std::ofstream master_dta("./temp/songs/songs.dta", std::ios_base::binary);

    for (const auto & entry : fs::directory_iterator("./cons")) {
        std::string path = entry.path();
        std::string path_yaml = "./temp/con/onyx-repack/repack-stfs.yaml";
        command_log.push_back(path.c_str());
        
        // extract con
        std::string command = onyx_path + " extract " + "'" + path + "'"  + " --to ./temp/con";
        system(command.c_str());

        if (fs::exists(path_yaml)) {
            // ensure con is for rb1/rb2/lrb
            YAML::Node config = YAML::LoadFile(path_yaml.c_str());
            int TitleID = config["title-id"].as<int>();
            //             rb1        /             rb2        /             lrb
            if (TitleID == 1161889833 || TitleID == 1161889897 || TitleID == 1464993776) {
                std::string dta_path = "./temp/con/songs/songs.dta";
                if (fs::exists(dta_path)) {
                    // merge dtas
                    std::ifstream dta_file(dta_path, std::ios_base::binary);
                    master_dta << dta_file.rdbuf();
                    // make all files lowercase so ugc will load as all ark files HAVE to be lowercase
                    for (const auto & files : fs::recursive_directory_iterator("./temp/con/songs")) {
                        if (!fs::is_directory(files.path())) {
                            std::string filename = files.path().filename().string();
                            // this is so fucking hacky
                            fs::rename(files.path(), search_replace(files.path().string(), filename, tolower(filename)).c_str());
                        }
                    }
                    for (const auto & files : fs::directory_iterator("./temp/con/songs")) {
                        if (fs::is_directory(files.path())) {
                            // move song folders
                            std::string target_path = "./temp/songs/" + files.path().filename().string();
                            fs::rename(files.path(), tolower(target_path).c_str());
                        }
                    }
                    command_log.push_back("succesfully extracted con");
                } else
                    command_log.push_back("no dta found in con");
            } else
                command_log.push_back("con is for unsupported game");
        } else
            command_log.push_back("failed to find yaml");

        // remove extracted dir
        fs::remove_all("./temp/con"); 
    }
    for (const auto & files : fs::recursive_directory_iterator("./temp/songs")) {
        // convert milos and lipsync in them to little endian
        if (files.path().filename().string().contains(".milo_xbox")) {
            std::string milo_path = files.path().string();
            std::string ext_path = milo_path + "_extract";

            std::string wmp = search_replace(milo_path, ".milo_xbox", ".milo_wii");

            // you may ask why am i using both.
            // superfreq sux at extracting milos and claims they dont exist most of the time.
            // scene_tool hard codes the milo version and endianess to something lrb wii doesn't support.
            std::string sc_command = scene_tool_path + " milo2dir " + milo_path + " " + ext_path;            
            std::string sf_command = superfreq_path + " dir2milo " + ext_path + " " + wmp + " -m 25 -p wii";
            std::string lipdir_path = ext_path + "/ObjectDir/lipsync";

            system(sc_command.c_str());
            fs::remove(milo_path);
            fs::remove(lipdir_path);
            fs::copy(objdir_path, lipdir_path);
            for (const auto & milofiles : fs::recursive_directory_iterator(ext_path)) {
                if (milofiles.path().filename().string().contains(".lipsync")) {
                    fix_lipsync(milofiles.path().string(), milofiles.path().string());
                }
            }
            system(sf_command.c_str());
            fs::remove_all(ext_path);
        }
        /* convert album art to wii tpls
        if (files.path().filename().string().contains("_keep.png_xbox")) {
            std::string png_path = files.path().string();
            std::string ext_path = png_path + ".png";

            std::string wpp = png_path;
            wpp = std::regex_replace(wpp, std::regex("\\_keep.png_xbox"), "_nomip_keep.bmp");

            std::string sfe_command = superfreq_path + " tex2png " + png_path + " " + ext_path + " -m 25 -b -p x360";
            std::string sfc_command = superfreq_path + " png2tex " + ext_path + " " + wpp + " -m 25 -p wii";
            system(sfe_command.c_str());
            system(sfc_command.c_str());
        }
        */
    }
    command_log.push_back("converted all cons");
    /*for (const auto & entry : fs::directory_iterator("./wads")) {
        std::string path = entry.path();
        command_log.push_back(path.c_str());
    }
    command_log.push_back("extracted all wads");*/
    //fs::remove_all("./temp");
}

// this is ai generated based off ai genrated python code. :3
// nothing else in this is ai generated i've been having a lot of !fun learning c++
void fix_lipsync(const std::string &input_file, const std::string &output_file) {
    // Read file into memory
    std::ifstream in(input_file, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open " << input_file << "\n";
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
    in.close();

    if (data.size() < 8) {
        std::cerr << "File too small.\n";
        return;
    }

    // Detect endian
    char endian;
    if (data[4] == 0x02 && data[5] == 0x00 && data[6] == 0x00 && data[7] == 0x00) {
        endian = '<'; // little endian
    } else if (data[4] == 0x00 && data[5] == 0x00 && data[6] == 0x00 && data[7] == 0x02) {
        endian = '>'; // big endian
    } else {
        std::cerr << "Unknown version field. Cannot determine endianness.\n";
        return;
    }

    auto flip_uint32 = [&](size_t offset) {
        if (offset + 4 > data.size()) return;
        uint32_t value_be = (data[offset] << 24) | (data[offset+1] << 16) |
                            (data[offset+2] << 8) | data[offset+3];
        uint8_t flipped[4];
        if (endian == '>') { // big endian -> little
            flipped[0] = (value_be) & 0xFF;
            flipped[1] = (value_be >> 8) & 0xFF;
            flipped[2] = (value_be >> 16) & 0xFF;
            flipped[3] = (value_be >> 24) & 0xFF;
        } else { // little endian -> big
            flipped[0] = (value_be >> 24) & 0xFF;
            flipped[1] = (value_be >> 16) & 0xFF;
            flipped[2] = (value_be >> 8) & 0xFF;
            flipped[3] = (value_be) & 0xFF;
        }
        for (int i = 0; i < 4; i++) data[offset + i] = flipped[i];
    };

    // Flip version stuff
    flip_uint32(0x4);
    flip_uint32(0x11);
    flip_uint32(0x0);

    // Flip string lengths
    size_t pos = 0x15;
    size_t max_len = data.size();
    size_t last_string_end = 0;

    auto read_uint32 = [&](size_t p, char e) -> uint32_t {
        if (e == '<') // little endian
            return (uint32_t)data[p] | ((uint32_t)data[p+1] << 8) |
                   ((uint32_t)data[p+2] << 16) | ((uint32_t)data[p+3] << 24);
        else // big endian
            return ((uint32_t)data[p] << 24) | ((uint32_t)data[p+1] << 16) |
                   ((uint32_t)data[p+2] << 8) | (uint32_t)data[p+3];
    };

    auto write_uint32 = [&](size_t p, uint32_t val, char e) {
        if (e == '<') {
            data[p] = val & 0xFF;
            data[p+1] = (val >> 8) & 0xFF;
            data[p+2] = (val >> 16) & 0xFF;
            data[p+3] = (val >> 24) & 0xFF;
        } else {
            data[p] = (val >> 24) & 0xFF;
            data[p+1] = (val >> 16) & 0xFF;
            data[p+2] = (val >> 8) & 0xFF;
            data[p+3] = val & 0xFF;
        }
    };

    char endian_out = (endian == '>') ? '<' : '>';

    while (pos + 4 <= max_len) {
        uint32_t orig_len = read_uint32(pos, endian);
        if (orig_len > 0x1000 || pos + 4 + orig_len > max_len) {
            last_string_end = pos;
            break;
        }
        write_uint32(pos, orig_len, endian_out);
        pos += 4 + orig_len;
        last_string_end = pos;
    }

    if (last_string_end) {
        std::cout << "Bytes after string section at 0x"
                  << std::hex << last_string_end << ":\n";
        for (size_t i = last_string_end; i < last_string_end + 16 && i < max_len; i++) {
            std::cout << std::setw(2) << std::setfill('0') << std::uppercase
                      << std::hex << (int)data[i] << " ";
        }
        std::cout << "\n";

        if (last_string_end + 8 <= max_len) {
            uint32_t val1 = read_uint32(last_string_end, endian);
            uint32_t val2 = read_uint32(last_string_end + 4, endian);
            write_uint32(last_string_end, val1, endian_out);
            write_uint32(last_string_end + 4, val2, endian_out);
        }
    }

    // Write output
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to write " << output_file << "\n";
        return;
    }
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();

    std::cout << "Converted " << input_file << " -> " << output_file
              << " (flipped header and string lengths)\n";
}
