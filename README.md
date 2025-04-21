### script
```bash
rmdir /s /q build
cmake -S . -B build -A x64

cmake --build build --config Release --parallel
```