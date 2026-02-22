{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    raylib
    gcc
    gnumake
    pkg-config
    libGL
    xorg.libX11
  ];

  shellHook = ''
    echo "Ambiente Apple Juice OOP carregado."
    echo "Comandos disponíveis:"
    echo "  make        — compila o simulador"
    echo "  make run    — executa o simulador"
    echo "  make test   — roda os testes"
    echo "  make clean  — limpa binários"
  '';
}
