# Parallel Heat Transfer Simulation with Visualization

A distributed parallel simulation of 2D heat diffusion using MPI with optional real-time OpenGL visualization.

## Overview

This project simulates heat transfer across a 2D sheet using the finite difference method. The simulation domain is divided among 4 MPI processes, with each process computing heat diffusion for its portion of the sheet. The simulation includes optional real-time visualization showing the temperature distribution with a color gradient from blue (cold) to red (hot).

## Features

- **Parallel Computing**: Uses MPI to distribute computation across 4 processes
- **2D Heat Diffusion**: Implements the heat equation using finite difference method
- **Real-time Visualization**: Optional OpenGL rendering showing heat distribution
- **Convergence-based Termination**: Simulation runs until convergence criterion is met
- **Domain Decomposition**: Sheet divided into 4 quadrants for parallel processing

## Requirements

### Required Libraries
- MPI implementation (OpenMPI, MPICH, etc.)
- OpenGL 3.3+
- GLFW3
- GLAD (OpenGL loader)

### Build Tools
- C compiler with C99 support
- MPI compiler wrapper (mpicc)

## Building

```bash
# Basic compilation
mpicc -o heat_sim heat_vis_test.c -lglfw -lGL -lm -ldl

# With optimization
mpicc -O3 -o heat_sim heat_vis_test.c -lglfw -lGL -lm -ldl
```

## Usage

### Running without visualization (faster):
```bash
mpirun -np 4 ./heat_sim
```

### Running with real-time visualization:
```bash
mpirun -np 4 ./heat_sim --visualize
```

**Note**: The program must be run with exactly 4 MPI processes.

## Configuration

Key parameters can be modified in the source code:

```c
#define N 14          // Grid size (NxN sheet)
#define ALPHA 0.125   // Thermal diffusivity coefficient
#define EPSILON 0.05  // Convergence threshold
```

## How It Works

### Domain Decomposition
The simulation divides the sheet into 4 quadrants:
- **Rank 0**: Top-left quadrant
- **Rank 1**: Top-right quadrant
- **Rank 2**: Bottom-left quadrant
- **Rank 3**: Bottom-right quadrant

### Initial Conditions
Heat sources are placed along the left edge (ranks 0 and 2) with an initial temperature of 100.

### Algorithm
1. Each process initializes its portion of the sheet
2. Iteratively compute heat diffusion using the finite difference method:
   - Exchange boundary values with neighboring processes
   - Update temperature at each interior point
   - Calculate local convergence error
3. Global synchronization to check convergence across all processes
4. Continue until global error is below threshold

### Visualization
When enabled, each MPI process opens its own window showing its portion of the heat distribution. Windows are automatically positioned in a 2×2 grid layout. Press ESC to close windows and terminate simulation.

## Output

The program prints the final heat distribution matrix to stdout from rank 0, showing integer temperature values across the entire sheet.

## Performance Notes

- Visualization significantly slows down the simulation (updates every 5 iterations)
- For performance benchmarking, run without visualization
- Larger grid sizes (increase N) will require more iterations to converge

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]

## Author

[Add your information here]
