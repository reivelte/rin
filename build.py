#!/usr/bin/env python3

import argparse
import subprocess
import glob
import os

def get_user_response(msg: str, valid_responses: dict) -> str:
    response = ""
    valid_response_inputs = set([r.lower() for r in valid_responses.keys()])
    while True:
        try:
            response = input(f'{msg}')
            if not response.lower() in valid_response_inputs:
                print(f'Invalid response. Choose from: {valid_response_inputs}')
                continue
        except ValueError:
            print(f'Invalid response. Choose from: {valid_response_inputs}')
            continue
        else:
            break
    return valid_responses[response]

# returns the difference in size of the lines list
def insert_new_list_items(start: int, items: list, lines: list, proj_name: str) -> int:
    orig_size = len(lines)
    i = start
    while not '#!END:' in lines[i]:
        line = lines[i].strip()
        if line.startswith("${") and line.endswith("}"):
            i += 1
        else:
            lines.pop(i)
    
    for f in items:
        lines.insert(i, f'  {f.replace(proj_name, "${PROJECT_NAME}")}')
        i += 1
    new_size = len(lines)

    return new_size - orig_size

# expects source files to be a subdir of root_dir
def glob_files(type: str, root_dir: str) -> list[str]:
    cwd = os.getcwd()
    os.chdir(root_dir)
    files = [file.replace("\\", "/") for file in glob.glob(f'src/**/**.{type}', recursive=True)] # keep it consistent across Windows and posix
    os.chdir(cwd)
    return files

# returns the number of new items found, and number of items deleted
def print_new_list_items(orig_list: list, new_list: list) -> tuple[int, int]:
    new = 0
    deleted = 0
    for f in new_list: # as globbed from OS
        if not f in orig_list:
            print(f'new: {f}')
            new += 1
    for f in orig_list: # likely has posix style paths
        line = f.strip()
        if line.startswith("${") and line.endswith("}"):
            continue
        if not f in new_list:
            print(f'deleted: {f}')
            deleted += 1
    return (new, deleted)


def update_cmake_list(path: str, proj_name: str, force: bool = False) -> None:
    lines = []
    headers_from_list = []
    sources_from_list = []
    
    with open(path, mode='rt') as f:
        lines = [line.rstrip() for line in f.readlines()]
    
    start = 0
    end = start
    
    headers_start = 0
    sources_start = 0
    headers_found = False
    sources_found = False
    
    for i, line in enumerate(lines):
        if '#!BEGIN:' in line:
            start = i + 1
        elif '#!END:' in line:
            end = i
        elif start < end:
                # NOTE: we assume there are at most two source lists being set in the given CMakeLists.txt file
                files = [file.strip().replace("${PROJECT_NAME}", proj_name) for file in lines[start:end]]
                if 'header list' in lines[start - 1].lower():
                    if not headers_found:
                        headers_from_list.extend(files)
                        headers_start = start
                        headers_found = True
                else:
                    if not sources_found:
                        sources_from_list.extend(files)
                        sources_start = start
                        sources_found = True
                start = end
                if headers_found and sources_found:
                    break # nothing left to do
    
    root_dir = os.path.dirname(path)
    current_headers = glob_files('hpp', root_dir=root_dir)
    current_sources = glob_files('cpp', root_dir=root_dir)

    new_hh = 0
    deleted_hh = 0
    
    new_cc = 0
    deleted_cc = 0
    
    if headers_found:
        new_hh, deleted_hh = print_new_list_items(orig_list=headers_from_list, new_list=current_headers)
    else:
        # headers are grouped with sources
        current_sources.extend(current_headers)
    if sources_found:
        new_cc, deleted_cc = print_new_list_items(orig_list=sources_from_list, new_list=current_sources)
    total_new = new_hh + new_cc
    total_deleted = deleted_hh + deleted_cc

    if total_new == 0 and total_deleted == 0:
        print(f'Path: {path} - No changes detected in source lists. Doing nothing.')
        return
    else:
        print(f'Path: {path} - {total_new} new file(s). {total_deleted} deleted file(s).')
    
    if not force:
        if total_new or total_deleted:
            response = get_user_response(msg="Proceed with changes? (y/n): ",
                valid_responses={
                    'y': 'y',
                    'yes': 'y',
                    'n': 'n',
                    'no': 'n'
                })
            if response == 'n':
                print(f'Exiting with no changes.')
                exit(code=1)
    
    # this only works because we assume there are at most two source lists being set in CMakeLists.txt (one for headers, one for sources)
    # it is assummed that headers are listed before sources
    diff = 0
    if headers_found:
        diff = insert_new_list_items(start=headers_start, items=current_headers, lines=lines, proj_name=proj_name)
    insert_new_list_items(start=sources_start + diff, items=current_sources, lines=lines, proj_name=proj_name)

    with open(path, mode='wt', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    if total_new or total_deleted:
        print(f'Updated: {path}')

def sh(tokens: list, capture_output: bool = False) -> int:
    proc = None
    try:
        proc = subprocess.run(tokens, capture_output=capture_output)
    except FileNotFoundError as e:
        return -1
    except Exception as e:
        print(e)
        return -1
    return proc.returncode

def main():
    parser = argparse.ArgumentParser(description="Detects OS and presence of required build tools, finds sources to build, and executes build scripts.")
    parser.add_argument("build_type", nargs="?", default="debug", choices=["debug", "release"], help="Specify debug or release version to build")
    parser.add_argument("-v", "--verbose", action="store_const", const=True, help="Pass --verbose to cmake --build")
    parser.add_argument("-m", "--make-only", dest="make_only", action="store_const", const=True, help="Only generate build files")
    parser.add_argument("-c", "--compile-only", dest="compile_only", action="store_const", const=True, help="Only compile using prexisiting build files")
    parser.add_argument("-r", "--reglob", action="store_const", const=True, help="Detect any differences between source list in CMakeLists.txt and what is present on the filesystem and update the CMakeLists.txt file accordingly")
    parser.add_argument("-f", "--force", action="store_const", const=True, help="Ignore prompts for confirmation. Use with -r")
    parser.add_argument("-t", "--tests", dest="tests", action="store_const", const=True, help="Pass -DSZ_BUILD_TESTS=ON to cmake in order to build the testing binary for suzuri")
    parser.add_argument("-s", "--static", dest="static", action="store_const", const=True, help="Build suzuri as a static library")
    parser.add_argument("--use-conan", dest="use_conan", action="store_const", const=True, help="Force the use of Conan regardless of OS. Use if on a posix system and Nix is not installed")

    args = parser.parse_args()

    if args.reglob:
        update_cmake_list(path='lib/CMakeLists.txt', proj_name='suzuri', force=args.force)
        update_cmake_list(path='cli/CMakeLists.txt', proj_name='rin', force=args.force)
        update_cmake_list(path='gui/CMakeLists.txt', proj_name='rin-gui', force=args.force)

    use_conan = args.use_conan
    if (os.name == "posix") and not use_conan:
        rc = sh(["nix", "--version"], capture_output=True)
        if rc != 0:
            print('Warning: Nix does not appear to be installed. Defaulting to Conan for package management')
            use_conan = True
        else:
            if not os.getenv(key='name') == 'nix-shell-env':
                print('Error: Nix devshell not detected. build.py expects to be executed inside a Nix devshell (via `nix develop`) on posix systems. Pass --use-conan to force use of conan for package management if you do not want to use Nix.')
                exit(code=-1)
    
    cmake_build_type = "Release" if args.build_type == "release" else "Debug"
    cmds = []
    if (os.name == "posix") and not use_conan:
        # Requirements:
        # nix (package management handled by nix, this script should only be executed when inside a nix devshell (using 'nix develop' in the same dir as flake.nix))    
        cmake_build_dir = os.path.join(os.getcwd(), "build", cmake_build_type.lower())
        os.makedirs(cmake_build_dir, exist_ok=True)
        if not args.compile_only:
            cmds.append(["cmake", "--fresh", "-B", cmake_build_dir, f"-DCMAKE_BUILD_TYPE={cmake_build_type}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=1"])
            if args.tests:
                cmds[0].append("-DSZ_BUILD_TESTS=ON")
            if args.static:
                cmds[0].append("-DSZ_SHARED_LIBS=OFF")
        
        if not args.make_only:
            cmds.append(["cmake", "--build", cmake_build_dir, "-j", str(os.cpu_count())] + (["--verbose"] if args.verbose else []))
    elif (os.name == "nt") or use_conan:
        # Requirements:
        # uvx or conan (package management handled by conan, things might be problematic if the profile is custom in any way)
        rc = sh(["conan", "--version"], capture_output=True)
        use_uvx = False
        if rc != 0:
            print("Warning: conan not found, attempting to execute it via uvx")
            rc = sh(["uvx", "--version"], capture_output=True)
            if rc == 0:
                use_uvx = True
            else:
                print("Error: could not find a useable package manager. Exiting.")
                exit(code=-1)
        
        if not args.compile_only:
            cmds.append(["conan", "profile", "detect", "--exist-ok"])
            cmds.append(["conan", "install", ".", f'-s=build_type={cmake_build_type}', "-of=build", "--build=missing"])
            if use_uvx:
                for i in range(len(cmds)):
                    if cmds[i][0] == "conan":
                        cmds[i].insert(0, "uvx")
            
            rc = sh(["cmake", "--version"], capture_output=True)
            if rc != 0:
                print("Error: cmake not found. Exiting.")
                exit(code=-1)
            
            omit_fresh = False
            proc = subprocess.run(["cmake", "--version"], capture_output=True)
            maj, min, patch = [int(val) for val in proc.stdout.split(b'\n')[0].split(b' ')[-1].split(b'.')]
            if (maj < 4) and (min < 24):
                print("Warning: omitting --fresh in call to cmake because the used version does not support it.")
                omit_fresh = True
            
            cmds.append(["cmake", "--preset conan-default"] + (["--fresh"] if not omit_fresh else []))
            if args.tests:
                cmds[2].append("-DSZ_BUILD_TESTS=ON")
            if args.static:
                cmds[2].append("-DSZ_SHARED_LIBS=OFF")

        if not args.make_only:
            cmds.append(["cmake", "--build", f"--preset conan-{cmake_build_type.lower()}"] + (["--verbose"] if args.verbose else []))
    else:
        print(f"Unsupported operating system: {os.name}")
        exit(code=-1)

    for cmd in cmds:
        print(f"Executing: {cmd}")
        rc = sh(cmd)
        if rc != 0:
            print(f"Exiting with non-zero exitcode {rc}")
            exit(rc)

if __name__ == '__main__':
    main()