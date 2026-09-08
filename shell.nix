with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "decker-build-env";
    buildInputs = [ unixtools.xxd SDL3 SDL3_image ];
}
