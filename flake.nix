{
  description = "sensor-fusion development environment and build";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachSystem [
      "x86_64-linux"
      "aarch64-linux"
      "x86_64-darwin"
      "aarch64-darwin"
    ] (system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = pkgs.lib;
        eigenInclude = "${pkgs.eigen}/include/eigen3";
        pkgConfigPath = lib.makeSearchPath "lib/pkgconfig" [
          pkgs.grpc
          pkgs.openssl
          pkgs.protobuf
          pkgs.re2
          pkgs.zlib
        ];

        sensorFusion = pkgs.clangStdenv.mkDerivation {
          pname = "sensor-fusion";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];

          buildInputs = [
            pkgs.eigen
            pkgs.grpc
            pkgs.openssl
            pkgs.protobuf
            pkgs.re2
            pkgs.zlib
          ];

          cmakeFlags = [
            "-GNinja"
            "-DEIGEN_LOCAL_SOURCE=${eigenInclude}"
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          doCheck = true;
          checkPhase = ''
            ctest --output-on-failure
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p "$out/bin"
            for exe in \
              run_sim run_replay run_bench run_profile run_cluster_demo run_load_test \
              telemetry_ingest sensor_node run_incident_replay tests; do
              if [ -x "$exe" ]; then
                cp "$exe" "$out/bin/"
              fi
            done
            runHook postInstall
          '';
        };
      in {
        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.clang
            pkgs.cmake
            pkgs.eigen
            pkgs.git
            pkgs.grpc
            pkgs.ninja
            pkgs.pkg-config
            pkgs.protobuf
            pkgs.python3
          ];

          shellHook = ''
            export CC=clang
            export CXX=clang++
            export CMAKE_GENERATOR=Ninja
            export EIGEN_LOCAL_SOURCE=${eigenInclude}
            export PKG_CONFIG_PATH=${pkgConfigPath}:''${PKG_CONFIG_PATH:-}
          '';
        };

        packages.default = sensorFusion;
        checks.default = sensorFusion;
      });
}
