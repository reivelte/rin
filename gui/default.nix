{ 
    stdenv, lib,
    cmake, pkg-config,
    sqlite, botan3,
    suzuri, rin,
    qt6
}:
stdenv.mkDerivation {
    name = "rin-gui";
    
    nativeBuildInputs = [
        cmake
        pkg-config
        qt6.wrapQtAppsHook
    ];
    
    buildInputs = [
        sqlite
        botan3
        suzuri
        qt6.qtbase
        qt6.qtsvg
    ];
    
    src = ./.;

    installPhase = ''
        mkdir -p $out/bin
        cp ${rin}/bin/rin $out/bin/
        cp rin-gui $out/bin/
    '';
}