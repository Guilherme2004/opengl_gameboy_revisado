# Game Boy Parallax - OpenGL C++

Jogo 2D em C++ com OpenGL, GLFW e GLEW.

## Controles

- Setas ou WASD: mover o personagem
- ESC: fechar o jogo

## Como compilar no Ubuntu/Linux

Instale as dependências:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libgl1-mesa-dev libglfw3-dev libglew-dev
```

Compile:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Execute:

```bash
./gameboy_parallax
```

## Estrutura

```text
opengl_gameboy/
├── CMakeLists.txt
├── README.md
└── src/
    └── main.cpp
```

O cenário, o personagem e o HUD são gerados por código em texturas RGBA, sem depender de imagens externas.
