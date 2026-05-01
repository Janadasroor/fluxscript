/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

// flux-pkg.cpp - FluxScript Package Manager CLI
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "flux/tooling/package_manager.h"

void printHelp() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "       FLUXSCRIPT PACKAGE MANAGER                       \n";
    std::cout << "\n";
    std::cout << " Usage: flux-pkg <command> [options]                    \n";
    std::cout << "\n";
    std::cout << " Commands:                                              \n";
    std::cout << "   search <query>      Search for packages              \n";
    std::cout << "   install <package>   Install a package                \n";
    std::cout << "   uninstall <pkg>     Uninstall a package              \n";
    std::cout << "   update [package]    Update packages                  \n";
    std::cout << "   list                List installed packages          \n";
    std::cout << "   info <package>      Show package information         \n";
    std::cout << "   repo add <name> <url>  Add repository                \n";
    std::cout << "   repo remove <name>     Remove repository             \n";
    std::cout << "   repo list              List repositories             \n";
    std::cout << "\n";
    std::cout << " Options:                                               \n";
    std::cout << "   -h, --help          Show this help                   \n";
    std::cout << "   -v, --version       Show version                     \n";
    std::cout << "   --registry <url>    Use custom registry              \n";
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  flux-pkg search math\n";
    std::cout << "  flux-pkg install math-lib\n";
    std::cout << "  flux-pkg update\n";
    std::cout << "  flux-pkg list\n";
    std::cout << "\n";
}

void printVersion() {
    std::cout << "FluxScript Package Manager v1.0.0\n";
    std::cout << "Copyright  2026 FluxScript Project\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp();
        return 0;
    }
    
    std::string command = argv[1];
    std::string registryUrl = "https://packages.fluxscript.org";
    
    // Parse global options
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--registry" && i + 1 < argc) {
            registryUrl = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        }
    }
    
    // Initialize package manager
    Flux::PackageManager::instance().initialize(registryUrl);
    
    // Execute command
    if (command == "search") {
        std::string query = (argc > 2) ? argv[2] : "*";
        auto results = Flux::PackageManager::instance().search(query);
        
    } else if (command == "install") {
        if (argc < 3) {
            std::cerr << "Error: Package name required\n";
            std::cerr << "Usage: flux-pkg install <package> [version]\n";
            return 1;
        }
        std::string packageName = argv[2];
        std::string version = (argc > 3) ? argv[3] : "";
        
        auto status = Flux::PackageManager::instance().install(packageName, version);
        
        if (status == Flux::InstallStatus::InstallSuccess) {
            std::cout << "\n Package installed successfully!\n";
        } else if (status == Flux::InstallStatus::InstallAlreadyInstalled) {
            std::cout << "\n Package is already installed.\n";
        } else {
            std::cerr << "\n Failed to install package.\n";
            return 1;
        }
        
    } else if (command == "uninstall") {
        if (argc < 3) {
            std::cerr << "Error: Package name required\n";
            return 1;
        }
        std::string packageName = argv[2];
        Flux::PackageManager::instance().uninstall(packageName);
        
    } else if (command == "update") {
        std::string packageName = (argc > 2) ? argv[2] : "";
        Flux::PackageManager::instance().update(packageName);
        
    } else if (command == "list") {
        std::cout << "\n\n";
        std::cout << "          INSTALLED PACKAGES                            \n";
        std::cout << "\n";
        
        auto packages = Flux::PackageManager::instance().listInstalled();
        
        if (packages.empty()) {
            std::cout << "   No packages installed.                              \n";
        } else {
            for (const auto& pkg : packages) {
                std::string line = pkg.name + " v" + pkg.version;
                std::cout << "    " << std::left << std::setw(48) << line << "\n";
            }
        }
        
        std::cout << "\n";
        
    } else if (command == "info") {
        if (argc < 3) {
            std::cerr << "Error: Package name required\n";
            return 1;
        }
        std::string packageName = argv[2];
        auto info = Flux::PackageManager::instance().getPackageInfo(packageName);
        
        std::cout << "\n\n";
        std::cout << "          PACKAGE INFORMATION                           \n";
        std::cout << "\n";
        std::cout << " Name: " << std::left << std::setw(50) << info.name << "\n";
        std::cout << " Version: " << std::left << std::setw(47) << info.version << "\n";
        std::cout << " Description: " << std::left << std::setw(43) << info.description << "\n";
        std::cout << " Author: " << std::left << std::setw(48) << info.author << "\n";
        std::cout << " License: " << std::left << std::setw(47) << info.license << "\n";
        std::cout << "\n";
        
    } else if (command == "repo") {
        if (argc < 3) {
            std::cerr << "Error: Repository command required\n";
            std::cerr << "Usage: flux-pkg repo <add|remove|list> [args]\n";
            return 1;
        }
        
        std::string repoCommand = argv[2];
        
        if (repoCommand == "add") {
            if (argc < 5) {
                std::cerr << "Error: Name and URL required\n";
                std::cerr << "Usage: flux-pkg repo add <name> <url>\n";
                return 1;
            }
            std::string name = argv[3];
            std::string url = argv[4];
            Flux::PackageManager::instance().addRepository(name, url);
            
        } else if (repoCommand == "remove") {
            if (argc < 3) {
                std::cerr << "Error: Repository name required\n";
                return 1;
            }
            std::string name = argv[3];
            Flux::PackageManager::instance().removeRepository(name);
            
        } else if (repoCommand == "list") {
            std::cout << "\n\n";
            std::cout << "          REPOSITORIES                                  \n";
            std::cout << "\n";
            
            auto repos = Flux::PackageManager::instance().listRepositories();
            for (const auto& [name, url] : repos) {
                std::cout << "    " << std::left << std::setw(20) << name 
                          << std::setw(30) << url << "\n";
            }
            
            std::cout << "\n";
        }
        
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        printHelp();
        return 1;
    }
    
    // Cleanup
    Flux::PackageManager::instance().finalize();
    
    return 0;
}
