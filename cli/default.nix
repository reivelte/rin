{
    stdenv, lib,
    cmake, pkg-config,
    sqlite, botan3,
    suzuri
}:

stdenv.mkDerivation {
    name = "rin";
    
    nativeBuildInputs = [ 
        cmake
        pkg-config
    ];
    
    buildInputs = [
        sqlite
        botan3
        suzuri
    ];
    
    src = ./.;

    installPhase = ''
        mkdir -p $out/bin
        cp rin $out/bin/
    '';
}