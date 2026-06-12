{
    description = "flake for rin";
    inputs = {
        nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
		flake-parts.url = "github:hercules-ci/flake-parts";
        flake-root.url = "github:srid/flake-root";
    };
    outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
        imports = [ inputs.flake-root.flakeModule ];
		systems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
        perSystem = { lib, config, self', inputs', pkgs, system, ... }: {
            packages = rec {
                suzuri = pkgs.callPackage ./lib { stdenv = pkgs.gcc15Stdenv; static = false; };
                rin = pkgs.callPackage ./cli { stdenv = pkgs.gcc15Stdenv; suzuri = config.packages.suzuri; };
                rin-gui = pkgs.callPackage ./gui { stdenv = pkgs.gcc15Stdenv; suzuri = config.packages.suzuri; rin = config.packages.rin; };
                default = rin-gui;

            } // pkgs.lib.optionalAttrs (pkgs.stdenv.isDarwin) rec {
                suzuri = pkgs.callPackage ./lib { stdenv = pkgs.llvmPackages.libcxxStdenv; static = false; };
                rin = pkgs.callPackage ./cli { stdenv = pkgs.llvmPackages.libcxxStdenv; suzuri = config.packages.suzuri; };
                rin-gui = pkgs.callPackage ./gui { stdenv = pkgs.llvmPackages.libcxxStdenv; suzuri = config.packages.suzuri; rin = config.packages.rin; };
                default = rin-gui;
            };
            flake-root.projectRootFile = "flake.nix";
            devShells = let
                setupScript = with pkgs; with config.packages; ''
                    chmod +x $FLAKE_ROOT/build.py
                    alias build="$FLAKE_ROOT/build.py"
                    alias rin="$FLAKE_ROOT/build/debug/bin/rin"
                    alias rin-gui="$FLAKE_ROOT/build/debug/bin/rin-gui"
                    alias sz-test="$FLAKE_ROOT/build/debug/bin/suzuri-tests"
                    mkdir -p .vscode
                    cat > .vscode/c_cpp_properties.json <<EOF
                    {
                        "configurations": [
                            {
                                "name": "Nix DevShell",
                                "includePath": [
                                    "\''${workspaceFolder}/**",
                                    "\''${workspaceFolder}/build/debug/lib/src/${suzuri.name}",
                                    "\''${workspaceFolder}/lib/ext/nlohmann/json/include",
                                    "\''${workspaceFolder}/lib/ext/ToruNiina/toml11/include",
                                    ${(if stdenv.isLinux 
                                        then (lib.concatStrings [
                                            "\"${qt6.qtbase}/include\","
                                            "\"${qt6.qtsvg}/include\","
                                        ])
                                        
                                        else (lib.concatStrings [
                                            "\"${qt6.qtbase}/lib/\","
                                        ])
                                    )}
                                    "${catch2_3}/include",
                                    "${(lib.attrsets.getDev botan3)}/include/botan-3",
                                    "${(lib.attrsets.getDev sqlite)}/include"
                                ],
                                "defines": [],
                                "compilerPath": "${(if stdenv.isDarwin then clang_21 else gcc15)}/bin/${(if stdenv.isDarwin then "clang" else "g++")}",
                                "cStandard": "c23",
                                "cppStandard": "c++23",
                                "intelliSenseMode": "linux-gcc-x64"
                            }
                        ],
                        "version": 4
                    }
                    EOF
                    cat > .vscode/launch.json <<EOF
                    {
                        "version": "2.0.0",
                        "configurations": [
                            {
                                "name": "Nix Devshell",
                                "type": "cppdbg",
                                "request": "launch",
                                "program": "\''${workspaceFolder}/build/debug/bin/rin",
                                "args": [],
                                "stopAtEntry": true,
                                "cwd": "\''${fileDirname}",
                                "environment": [],
                                "preLaunchTask": "build_debug",
                                "sourceFileMap": {
                                    "\''${workspaceRoot}": {
                                        "editorPath" : "\''${workspaceFolder}",
                                        "useForBreakpoints" : true
                                    }
                                },
                                "linux": {
                                    "externalConsole": false,
                                    "MIMode": "gdb",
                                    "setupCommands": [
                                        {
                                            "description": "Enable pretty-printing for gdb",
                                            "text": "-enable-pretty-printing",
                                            "ignoreFailures": false
                                        },
                                        {
                                            "description": "Set Disassembly Flavor to Intel",
                                            "text": "-gdb-set disassembly-flavor intel",
                                            "ignoreFailures": true
                                        },
                                        {
                                            "description": "Set up compilation directories for GDB",
                                            "text": "-gdb-set directories \''${workspaceFolder}/cli/src:\''${workspaceFolder}/lib/src"
                                        }
                                    ]
                                },
                                "osx": {
                                    "externalConsole": false,
                                    "MIMode": "lldb"
                                },
                                "windows": {
                                    "externalConsole": false,
                                    "MIMode": "gdb"
                                }
                            },
                            {
                                "name": "Nix Devshell - GUI",
                                "type": "cppdbg",
                                "request": "launch",
                                "program": "\''${workspaceFolder}/build/debug/bin/rin-gui",
                                "args": [],
                                "stopAtEntry": false,
                                "cwd": "\''${fileDirname}",
                                "environment": [],
                                "preLaunchTask": "build_debug",
                                "sourceFileMap": {
                                    "\''${workspaceRoot}": {
                                        "editorPath" : "\''${workspaceFolder}",
                                        "useForBreakpoints" : true
                                    }
                                },
                                "linux": {
                                    "externalConsole": false,
                                    "MIMode": "gdb",
                                    "setupCommands": [
                                        {
                                            "description": "Enable pretty-printing for gdb",
                                            "text": "-enable-pretty-printing",
                                            "ignoreFailures": false
                                        },
                                        {
                                            "description": "Set Disassembly Flavor to Intel",
                                            "text": "-gdb-set disassembly-flavor intel",
                                            "ignoreFailures": true
                                        },
                                        {
                                            "description": "Set up compilation directories for GDB",
                                            "text": "-gdb-set directories \''${workspaceFolder}/cli/src:\''${workspaceFolder}/lib/src"
                                        }
                                    ]
                                },
                                "osx": {
                                    "externalConsole": false,
                                    "MIMode": "lldb"
                                },
                                "windows": {
                                    "externalConsole": false,
                                    "MIMode": "gdb"
                                }
                            }
                        ]
                    }
                    EOF
                    cat > .vscode/tasks.json <<EOF
                    {
                        "version": "2.0.0",
                        "tasks": [
                            {
                                "label": "build_debug",
                                "type": "shell",
                                "command": "\''${workspaceRoot}/build.py -r debug",
                                "group": {
                                    "kind": "build",
                                    "isDefault": true
                                },
                                "problemMatcher": []
                            },
                            {
                                "label": "build_release",
                                "type": "shell",
                                "command": "\''${workspaceRoot}/build.py -r release",
                                "group": {
                                    "kind": "build",
                                    "isDefault": false
                                },
                                "problemMatcher": []
                            }
                        ]
                    }
                    EOF
                    
                    ln -s $FLAKE_ROOT/build/debug/compile_commands.json $FLAKE_ROOT/compile_commands.json
                    ln -s ${qt6.qtsvg}/lib/qt-6/plugins/iconengines $FLAKE_ROOT/build/debug/bin/iconengines
                    ln -s ${qt6.qtsvg}/lib/qt-6/plugins/imageformats $FLAKE_ROOT/build/debug/bin/imageformats
                    
                    echo "Welcome to the Nix development shell for the reflexive-indexer project."
                    echo "build is aliased to '$FLAKE_ROOT/build.py'"
                    echo "sz-test is aliased to '$FLAKE_ROOT/build/debug/bin/suzuri-tests'"
                    echo "rin is aliased to '$FLAKE_ROOT/build/debug/bin/rin'"
                    echo "rin-gui is aliased to '$FLAKE_ROOT/build/debug/bin/rin-gui'"
                '';
                packageInputs = with config; with config.packages; [
                    flake-root.devShell # provides $FLAKE_ROOT in shell
                    suzuri
                    rin
                    rin-gui
                ];
                commonPackages = with pkgs; [
                    git
                    python314 # for build.py
                    catch2_3
                ];
            in {
                default = pkgs.mkShell.override { stdenv = pkgs.gcc15Stdenv; } {
                    inputsFrom = packageInputs;
                    shellHook = lib.concatStrings [
                        setupScript
                        ''
                            cat > .vscode/settings.json <<EOF
                            {
                                "clangd.arguments": [
                                    "--query-driver=${pkgs.gcc15}/bin/gcc"
                                ],
                                "[cpp]":{
                                    "editor.defaultFormatter": "ms-vscode.cpptools"
                                }
                            }
                            EOF
                            cat > .clangd <<EOF
                            CompileFlags:
                              Compiler: ${pkgs.gcc15}/bin/gcc
                            EOF
                        ''
                        ];
                    packages = with pkgs; commonPackages ++ [
                        gdb
                    ];
                };
            } // pkgs.lib.optionalAttrs (system == "x86_64-darwin" || system == "aarch64-darwin" ) {
                default = pkgs.mkShell.override { stdenv = pkgs.llvmPackages.libcxxStdenv; } {
                    inputsFrom = packageInputs;
                    shellHook = lib.concatStrings [
                        setupScript
                        ''
                            SDKROOT=${pkgs.apple-sdk_15}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
                            DEVELOPER_DIR=${pkgs.apple-sdk_15}
                        ''
                    ];
                    packages = with pkgs; commonPackages ++ [
                        lldb
                    ];
                };
            };
        };
    };
}