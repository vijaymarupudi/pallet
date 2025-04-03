#include <stdio.h>
#include "lua.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <string_view>
#include <string.h>
#include <sstream>
#include <vector>
#include <utility>
#include <optional>
#include <fstream>
#include <algorithm>

struct State {
  std::vector<std::pair<std::string, std::string>> files;
  void add_file(const char* rootdir, const char* parent_dir_path, const char* file_name) {
    std::string key;
    if (strcmp(parent_dir_path, ".") == 0) {
      key = std::string(file_name);
    } else {
      key = std::string(parent_dir_path);
      key.append("/");
      key.append(file_name);
    }
    char buf[8192];
    snprintf(buf, 8192, "%s/%s/%s", rootdir, parent_dir_path, file_name);
    std::stringstream ss;
    std::ifstream file (buf, std::ios::in | std::ios::binary);
    ss << file.rdbuf();
    files.push_back(std::pair(std::move(key), std::move(ss.str())));
  }
};

void process(State& state, const char* rootdir, const char* dirname) {
  char realdirname[8192];
  snprintf(realdirname, 8192, "%s/%s", rootdir, dirname);

  DIR* dir = opendir(realdirname);
  struct dirent* ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) { continue; }
    if (ent->d_type & DT_REG) {
      state.add_file(rootdir, dirname, ent->d_name);
    } else if (ent->d_type & DT_DIR) {
      char newdirname[8192];
      if (strcmp(dirname, ".") == 0) {
        snprintf(newdirname, 8192, "%s", ent->d_name);
      } else {
        snprintf(newdirname, 8192, "%s/%s", dirname, ent->d_name);
      }
      process(state, rootdir, newdirname);
    }
  }
  closedir(dir);
}

struct WriterState {
  std::string data;
};

static int writer(lua_State* L, const void* data, size_t len, void* ud) {
  ((WriterState*)ud)->data.append((const char*)data, len);
  return 0;
}

static std::string l_compile_to_bytecode(lua_State* L, const std::string& luaCode) {
  WriterState state;
  luaL_loadstring(L, luaCode.c_str());
  lua_dump(L, writer, &state, 0);
  lua_pop(L, 1);
  return std::move(state.data);
}



static void output_hex(const char* name, std::string& out,
                       std::vector<std::pair<std::string, std::string>>& pairs) {

  char buf[8192];
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
    return std::get<0>(a) < std::get<0>(b);
  });

  for (size_t index = 0; index < pairs.size(); index++) {
    const auto& [modName, bytecode] = pairs[index];
    snprintf(buf, 8192, "const unsigned char %s_%llu_data[] = {", name, index);
    out.append(buf);
    const unsigned char* data = static_cast<const unsigned char*>(bytecode.data());
    for (size_t i = 0; i < bytecode.size(); i++) {
      unsigned char d = data[i];
      snprintf(buf, 8192, "%02X", d);
      out.append("0x");
      out.append(buf, 2);
      if (i != bytecode.size() - 1) {
        out.append(", ");
      }
    }
    out.append("};\n");
  }


  snprintf(buf, 8192, "const struct filesystem_entry %s[%llu] = {\n", name, pairs.size());
  out.append(buf);

  for (size_t index = 0; index < pairs.size(); index++) {
    const auto& [modName, bytecode] = pairs[index];
    snprintf(buf, 8192, "  { \"%s\", %llu, &%s_%llu_data[0] },\n", modName.c_str(), bytecode.size(), name, index);
    out.append(buf);
    const unsigned char* data = static_cast<const unsigned char*>(bytecode.data());



  }

  out.append("};\n\n");
  snprintf(buf, 8192, "const unsigned int %s_count = %llu;\n\n", name, pairs.size());
  out.append(buf);
}



std::string generateContents(const char* dirname) {
  lua_State* L = luaL_newstate();
  State state;
  process(state, dirname, ".");
  std::vector<std::pair<std::string, std::string>> luaModules;
  std::vector<std::pair<std::string, std::string>> regularFiles;
  for (auto&& [fname, content] : state.files) {
    if (fname.ends_with(".lua")) {
      std::string modName = fname;
      for (size_t i = 0; i < modName.size(); i++) {
        if (modName[i] == '/') {
          modName[i] = '.';
        }
      }
      modName.resize(modName.size() - 4);
      std::string bytecode = l_compile_to_bytecode(L, content);
      fprintf(stderr, "Lua module: %s (%llu)\n", modName.c_str(), bytecode.size());
      luaModules.push_back(std::pair(std::move(modName), std::move(bytecode)));
    } else {
      fprintf(stderr, "File: %s (%llu)\n", fname.c_str(), content.size());
      regularFiles.push_back(std::pair(std::move(fname), std::move(content)));
    }
  }

  std::string fileContents = R"(struct filesystem_entry {
  const char* filename;
  unsigned int size;
  const unsigned char* contents;
};

)";

  output_hex("filesystem_lua_modules", fileContents, luaModules);
  output_hex("filesystem_files", fileContents, regularFiles);
  lua_close(L);
  return fileContents;
}

int main(int argc, char** argv) {
  const char* dirname = argv[1];
  auto out = generateContents(dirname);

  if (argc == 2) {
    fwrite(out.c_str(), 1, out.size(), stdout);
  } else if (argc == 3) {
    const char* outfile = argv[2];
    if (strcmp(outfile, "-") == 0) {
      fwrite(out.c_str(), 1, out.size(), stdout);
    } else {
      FILE* f = fopen(outfile, "w");
      fwrite(out.c_str(), 1, out.size(), f);
      fclose(f);
    }
  }

}
