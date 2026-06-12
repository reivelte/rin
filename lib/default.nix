{
    stdenv, lib,
    cmake, pkg-config, sqlite, botan3,
    apple-sdk_15 ? stdenv.isDarwin,
    static
}:

stdenv.mkDerivation {

    name = "suzuri";
    
    nativeBuildInputs = [ 
        cmake
        pkg-config
    ]
    ++ (lib.optionals stdenv.isDarwin [
        apple-sdk_15
    ]);

    buildInputs = [ 
        sqlite
        botan3
    ];

    src = ./.;

    cmakeFlags = [

    ] ++ lib.optionals (static) [
        "-DSZ_SHARED_LIBS=OFF"
    ];
}


