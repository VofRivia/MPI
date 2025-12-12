#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <mpi.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string.h>

#define N 14          // size of sheet, will be considered that it is square
#define ALPHA 0.125   // thermal diffusivity
#define EPSILON 0.05  // stopping condition/criterion

typedef float data_type;

// Global variables for OpenGL
GLFWwindow* window = NULL;
unsigned int shaderProgram;
unsigned int VAO, VBO, EBO;
int window_width = 600;
int window_height = 600;

// Shader sources
const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in float aTemp;\n"
    "out float temp;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * vec4(aPos, 1.0);\n"
    "   temp = aTemp;\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in float temp;\n"
    "void main()\n"
    "{\n"
    "   // Color gradient from blue (cold) to red (hot)\n"
    "   float t = temp / 100.0;\n"
    "   vec3 color = mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), t);\n"
    "   FragColor = vec4(color, 1.0);\n"
    "}\n\0";

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
}

int initOpenGL(int rank) {
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    // Create window with rank in title
    char title[100];
    sprintf(title, "Heat Transfer - Rank %d", rank);
    window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
    
    // Position windows based on rank (after window creation)
    if (window != NULL) {
        glfwSetWindowPos(window, (rank % 2) * (window_width + 10), (rank / 2) * (window_height + 40));
    }
    
    if (window == NULL) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    
    // Load OpenGL function pointers (updated for newer GLAD)
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }
    printf("Loaded OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    
    // Build and compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    // Check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        fprintf(stderr, "Vertex shader compilation failed: %s\n", infoLog);
    }
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        fprintf(stderr, "Fragment shader compilation failed: %s\n", infoLog);
    }
    
    // Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "Shader program linking failed: %s\n", infoLog);
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return 0;
}

void setupBuffers(int part) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    // We'll update the VBO data in updateVisualization
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, part * part * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    
    // Generate indices for triangles
    unsigned int *indices = (unsigned int*)malloc((part-1) * (part-1) * 6 * sizeof(unsigned int));
    int idx = 0;
    for (int i = 0; i < part-1; i++) {
        for (int j = 0; j < part-1; j++) {
            indices[idx++] = i * part + j;
            indices[idx++] = i * part + (j + 1);
            indices[idx++] = (i + 1) * part + j;
            
            indices[idx++] = i * part + (j + 1);
            indices[idx++] = (i + 1) * part + j;
            indices[idx++] = (i + 1) * part + (j + 1);
        }
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (part-1) * (part-1) * 6 * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    
    free(indices);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Temperature attribute
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void updateVisualization(data_type** mat, int part) {
    // Create vertex data: [x, y, z, temperature]
    float *vertices = (float*)malloc(part * part * 4 * sizeof(float));
    
    for (int i = 0; i < part; i++) {
        for (int j = 0; j < part; j++) {
            int idx = (i * part + j) * 4;
            vertices[idx + 0] = (float)j / part * 2.0f - 1.0f;  // x: -1 to 1
            vertices[idx + 1] = (float)i / part * 2.0f - 1.0f;  // y: -1 to 1
            vertices[idx + 2] = 0.0f;                           // z
            vertices[idx + 3] = mat[i][j];                      // temperature
        }
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, part * part * 4 * sizeof(float), vertices);
    
    free(vertices);
}

void renderVisualization(int part) {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(shaderProgram);
    
    // Simple orthographic projection
    float projection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);
    
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (part-1) * (part-1) * 6, GL_UNSIGNED_INT, 0);
    
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void initialize(data_type** mat, int rank, int size) {
    int part = ((N+2)/2)+1;
    
    for (size_t i = 0; i < part; i++) {
        for (size_t j = 0; j < part; j++) {
            mat[i][j] = 0.;
        }
    }
    
    // filling initial heat spots
    if(rank == 0 || rank == 2){
        for (size_t i = 0; i < part; i++) {
            mat[i][0] = 100.;
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

void copy(data_type** og, data_type** cpy, int size){
    for(size_t i = 0; i < size; ++i){
        for(size_t j = 0; j < size; ++j){
            cpy[i][j] = og[i][j];
        }
    }
}

void simulation(data_type** mat, int rank, int size, int visualize) {
    int part = ((N + 2)/2) + 1;
    int row = rank/2;
    int col = rank%2;
    int neigh_h = row*2 + (col+1)%2;
    int neigh_v = ((row+1)%2)*2 + col;
    
    data_type** old;
    old = (data_type**)malloc(sizeof(data_type*)*part);
    for (size_t i = 0; i < part; i++) {
        old[i] = (data_type*)malloc(sizeof(data_type)*part);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    copy(mat, old, part);
    MPI_Status status;

    data_type adj_h[part], adj_v[part], edge_h[part], edge_v[part];
    
    data_type global_eps = EPSILON + 1;
    data_type max_eps = EPSILON + 1;
    data_type eps = 0.0;
    data_type delta_T = 0.0;
    
    int iteration = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    
    while(global_eps > EPSILON && (!visualize || !glfwWindowShouldClose(window))){
        // Prepare edge data
        for (size_t i = 0; i < part; i++) {   
            edge_h[i] = mat[i][(1-col)*(part-2)+col];
            edge_v[i] = mat[(1-row)*(part-2)+row][i];
        }
        
        // Send/receive edge values
        MPI_Send(edge_h, part, MPI_FLOAT, neigh_h, 0, MPI_COMM_WORLD);
        MPI_Send(edge_v, part, MPI_FLOAT, neigh_v, 0, MPI_COMM_WORLD);
        MPI_Recv(adj_h, part, MPI_FLOAT, neigh_h, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(adj_v, part, MPI_FLOAT, neigh_v, 0, MPI_COMM_WORLD, &status);

        // Update received data
        for (size_t i = 0; i < part; i++) {
            mat[i][(1-col)*(part-1)] = adj_h[i];
            mat[(1-row)*(part-1)][i] = adj_v[i];
        }
        
        global_eps = 0.0;
        max_eps = 0.0;
        
        // Simulation on part of sheet
        for (size_t i = 1; i < part-1; i++) {
            for (size_t j = 1; j < part-1; j++) {
                delta_T = ALPHA*(old[i + 1][j] + old[i - 1][j] + old[i][j + 1] + old[i][j - 1] - (4*old[i][j]));
                
                mat[i][j] += delta_T;
                if(mat[i][j]==0){
                    eps = delta_T/(mat[i][j]+0.001);
                }else{
                    eps = delta_T/mat[i][j];
                }
                
                if(eps<0.0){
                    eps = -eps;
                }
                if(eps > max_eps){
                    max_eps = eps;
                }
            }
        }
        
        copy(mat, old, part);
        MPI_Allreduce(&max_eps, &global_eps, 1, MPI_FLOAT, MPI_MAX, MPI_COMM_WORLD);
        
        // Visualization update (every few iterations to not slow down simulation)
        if (visualize && iteration % 5 == 0) {
            processInput(window);
            updateVisualization(mat, part);
            renderVisualization(part);
        }
        
        iteration++;
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // Cleanup
    for (size_t i = 0; i < part; i++) {
        free(old[i]);
    }
    free(old);
}

void flatten(data_type **mat, data_type *vec, int n){
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            vec[i*n+j] = mat[i][j];
        }
    }
}

void unflatten(data_type **mat, data_type *vec, int n){
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            mat[i][j] = vec[i*n+j];
        }
    }
}

void collect(data_type **mat, data_type **part_1, data_type **part_2, data_type **part_3, data_type **part_4){
    int part = ((N + 2)/2) + 1;

    for(size_t i = 0; i < part-1; ++i){
        for(size_t j = 0; j < part-1; ++j){
            mat[i][j] = part_1[i][j];
        }
    }
    
    for(size_t i = 0; i < part-1; ++i){
        for(size_t j = 1; j < part; ++j){
            mat[i][j+part-2] = part_2[i][j];
        }
    }
    
    for(size_t i = 1; i < part; ++i){
        for(size_t j = 0; j < part-1; ++j){
            mat[i+part-2][j] = part_3[i][j];
        }
    }
    
    for(size_t i = 1; i < part; ++i){
        for(size_t j = 1; j < part; ++j){
            mat[i+part-2][j+part-2] = part_4[i][j];
        }
    }
}

void print(data_type** mat, int n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            printf(" %d ", (int)mat[i][j]);
        }
        putchar('\n');
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    MPI_Status stat;

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Check if visualization is requested (pass --visualize flag)
    int visualize = 0;
    int user_visualize = 0;
        if (argc > 1 && strcmp(argv[1], "--visualize") == 0) {
        user_visualize = 1;
    }
    int visualize = (world_rank == 0 && user_visualize);


    data_type **sheet, **sheet_part, **part_1, **part_2, **part_3, **part_4, *vec_part, *vec_1, *vec_2, *vec_3, *vec_4;
    int part = ((N + 2)/2) + 1;

    // Allocate memory for whole sheet on rank 0
    if (world_rank == 0) {
        sheet = (data_type**)malloc(sizeof(data_type*)*(N + 2));
        for (size_t i = 0; i < N + 2; i++) {
            sheet[i] = (data_type*)malloc(sizeof(data_type)*(N + 2));
        }

        part_2 = (data_type**)malloc(sizeof(data_type*)*part);
        part_3 = (data_type**)malloc(sizeof(data_type*)*part);
        part_4 = (data_type**)malloc(sizeof(data_type*)*part);
        for (size_t i = 0; i < part; i++) {
            part_2[i] = (data_type*)malloc(sizeof(data_type)*part);
            part_3[i] = (data_type*)malloc(sizeof(data_type)*part);
            part_4[i] = (data_type*)malloc(sizeof(data_type)*part);
        }

        vec_2 = (data_type*)malloc(sizeof(data_type)*part*part);
        vec_3 = (data_type*)malloc(sizeof(data_type)*part*part);
        vec_4 = (data_type*)malloc(sizeof(data_type)*part*part);
    } else {
        vec_part = (data_type*)malloc(sizeof(data_type)*part*part);
    }

    sheet_part = (data_type**)malloc(sizeof(data_type*)*part);
    for (size_t i = 0; i < part; i++) {
        sheet_part[i] = (data_type*)malloc(sizeof(data_type)*part);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    initialize(sheet_part, world_rank, world_size);
    MPI_Barrier(MPI_COMM_WORLD);

    // Initialize OpenGL for visualization
    if (visualize) {
        if (initOpenGL(world_rank) == 0) {
            setupBuffers(part);
        } else {
            visualize = 0;  // Disable visualization if initialization failed
        }
    }

    // Run simulation
    simulation(sheet_part, world_rank, world_size, visualize);

    // Collect results on rank 0
    if(world_rank == 0){
        MPI_Recv(vec_2, part*part, MPI_FLOAT, 1, 0, MPI_COMM_WORLD, &stat);
        MPI_Recv(vec_3, part*part, MPI_FLOAT, 2, 0, MPI_COMM_WORLD, &stat);
        MPI_Recv(vec_4, part*part, MPI_FLOAT, 3, 0, MPI_COMM_WORLD, &stat);

        unflatten(part_2, vec_2, part);
        unflatten(part_3, vec_3, part);
        unflatten(part_4, vec_4, part);

        collect(sheet, sheet_part, part_2, part_3, part_4);

        printf("\nFinal heat distribution:\n");
        print(sheet, N+2);
    } else {
        flatten(sheet_part, vec_part, part);
        MPI_Send(vec_part, part*part, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);
    }

    // Cleanup OpenGL
    if (visualize) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glfwTerminate();
    }

    MPI_Finalize();
    return 0;
}