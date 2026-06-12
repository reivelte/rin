{
    stdenv, lib,
    cmake, pkg-config, suzuri
}:

stdenv.mkDerivation {
    name = "rin";
    
    nativeBuildInputs = [ 
        cmake
        pkg-config
    ];
    
    buildInputs = [
        suzuri
    ];
    
    src = ./.;

    installPhase = ''
        mkdir -p $out/bin
        cp rin $out/bin/
    '';
}