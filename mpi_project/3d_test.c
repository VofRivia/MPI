#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <mpi.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string.h>

#define N 12          // size of cube (NxNxN)
#define ALPHA 0.05    // thermal diffusivity
#define EPSILON 0.01  // stopping condition

typedef float data_type;

// OpenGL globals
GLFWwindow* window = NULL;
GLFWwindow* full_window = NULL;  // Full cube view window
unsigned int shaderProgram;
unsigned int VAO, VBO, EBO;
unsigned int full_VAO, full_VBO, full_EBO;  // For full cube
int window_width = 800;
int window_height = 800;

// Camera - separate for each window
float camera_angle_x = 25.0f;
float camera_angle_y = 45.0f;
float camera_distance = 8.0f;
double last_mouse_x = 400.0;
double last_mouse_y = 400.0;
int mouse_dragging = 0;

float full_camera_angle_x = 25.0f;
float full_camera_angle_y = 45.0f;
float full_camera_distance = 12.0f;
double full_last_mouse_x = 400.0;
double full_last_mouse_y = 400.0;
int full_mouse_dragging = 0;

// Simple vertex shader - draws cubes at each point
const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aOffset;\n"
    "layout (location = 2) in float aTemp;\n"
    "out float temp;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   vec3 pos = aPos * 0.08 + aOffset;\n"  // Small cubes
    "   gl_Position = projection * view * vec4(pos, 1.0);\n"
    "   temp = aTemp;\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in float temp;\n"
    "void main()\n"
    "{\n"
    "   if (temp < 0.5) discard;\n"
    "   float t = clamp(temp / 100.0, 0.0, 1.0);\n"
    "   vec3 hot = vec3(1.0, 0.0, 0.0);\n"
    "   vec3 warm = vec3(1.0, 1.0, 0.0);\n"
    "   vec3 cool = vec3(0.0, 1.0, 1.0);\n"
    "   vec3 cold = vec3(0.0, 0.0, 1.0);\n"
    "   vec3 color;\n"
    "   if (t > 0.66) color = mix(warm, hot, (t - 0.66) * 3.0);\n"
    "   else if (t > 0.33) color = mix(cool, warm, (t - 0.33) * 3.0);\n"
    "   else color = mix(cold, cool, t * 3.0);\n"
    "   FragColor = vec4(color, 0.95);\n"
    "}\n\0";

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    window_width = width;
    window_height = height;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            if (window == full_window) {
                full_mouse_dragging = 1;
                glfwGetCursorPos(window, &full_last_mouse_x, &full_last_mouse_y);
            } else {
                mouse_dragging = 1;
                glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);
            }
        } else {
            if (window == full_window) {
                full_mouse_dragging = 0;
            } else {
                mouse_dragging = 0;
            }
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (window == full_window && full_mouse_dragging) {
        float dx = xpos - full_last_mouse_x;
        float dy = ypos - full_last_mouse_y;
        full_camera_angle_y += dx * 0.3f;
        full_camera_angle_x -= dy * 0.3f;
        if (full_camera_angle_x > 89.0f) full_camera_angle_x = 89.0f;
        if (full_camera_angle_x < -89.0f) full_camera_angle_x = -89.0f;
        full_last_mouse_x = xpos;
        full_last_mouse_y = ypos;
    } else if (mouse_dragging) {
        float dx = xpos - last_mouse_x;
        float dy = ypos - last_mouse_y;
        camera_angle_y += dx * 0.3f;
        camera_angle_x -= dy * 0.3f;
        if (camera_angle_x > 89.0f) camera_angle_x = 89.0f;
        if (camera_angle_x < -89.0f) camera_angle_x = -89.0f;
        last_mouse_x = xpos;
        last_mouse_y = ypos;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (window == full_window) {
        full_camera_distance -= yoffset * 0.5f;
        if (full_camera_distance < 2.0f) full_camera_distance = 2.0f;
        if (full_camera_distance > 30.0f) full_camera_distance = 30.0f;
    } else {
        camera_distance -= yoffset * 0.5f;
        if (camera_distance < 2.0f) camera_distance = 2.0f;
        if (camera_distance > 30.0f) camera_distance = 30.0f;
    }
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
}

int initOpenGL(int rank) {
    if (!glfwInit()) return -1;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    char title[100];
    sprintf(title, "3D Heat - Rank %d (Drag to rotate, scroll to zoom)", rank);
    window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwSetWindowPos(window, (rank % 2) * 820, (rank / 2) * 50);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    
    if (gladLoadGL(glfwGetProcAddress) == 0) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    
    // Compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Create full cube window for rank 0
    if (rank == 0) {
        sprintf(title, "FULL CUBE VIEW (All Ranks Combined)");
        full_window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
        if (!full_window) {
            fprintf(stderr, "Failed to create full view window\n");
            return -1;
        }
        glfwSetWindowPos(full_window, 820, 450);
        
        // Share context with main window for shader reuse
        glfwMakeContextCurrent(full_window);
        glfwSetFramebufferSizeCallback(full_window, framebuffer_size_callback);
        glfwSetMouseButtonCallback(full_window, mouse_button_callback);
        glfwSetCursorPosCallback(full_window, cursor_position_callback);
        glfwSetScrollCallback(full_window, scroll_callback);
        
        // Switch back to main window
        glfwMakeContextCurrent(window);
    }
    
    return 0;
}

void setupCubeBuffers() {
    // Simple cube vertices
    float cube_verts[] = {
        -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,  // back
        -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1   // front
    };
    
    unsigned int cube_inds[] = {
        0,1,2, 2,3,0,  4,5,6, 6,7,4,  // back, front
        0,1,5, 5,4,0,  2,3,7, 7,6,2,  // bottom, top
        0,3,7, 7,4,0,  1,2,6, 6,5,1   // left, right
    };
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_inds), cube_inds, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Setup for full cube view (same buffers, separate VAO)
    glGenVertexArrays(1, &full_VAO);
    glGenBuffers(1, &full_VBO);
    glGenBuffers(1, &full_EBO);
    
    glBindVertexArray(full_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, full_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, full_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_inds), cube_inds, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void renderCubes(data_type*** mat, int part, GLFWwindow* target_window, int use_full_vao, float cam_x_angle, float cam_y_angle, float cam_dist) {
    glfwMakeContextCurrent(target_window);
    
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(shaderProgram);
    
    // View matrix - simple camera positioned to see the cube
    float rad_x = cam_x_angle * 3.14159f / 180.0f;
    float rad_y = cam_y_angle * 3.14159f / 180.0f;
    float cam_x = cam_dist * cosf(rad_x) * sinf(rad_y);
    float cam_y = cam_dist * sinf(rad_x);
    float cam_z = cam_dist * cosf(rad_x) * cosf(rad_y);
    
    // Simple view matrix (was working before)
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, -cam_x,-cam_y,-cam_z,1};
    
    // Projection
    float fov = 60.0f * 3.14159f / 180.0f;
    float aspect = (float)window_width / (float)window_height;
    float n = 0.1f, f = 100.0f;
    float fd = 1.0f / tanf(fov / 2.0f);
    float projection[16] = {
        fd/aspect,0,0,0, 0,fd,0,0, 0,0,(f+n)/(n-f),-1, 0,0,(2*f*n)/(n-f),0
    };
    
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);
    
    glBindVertexArray(use_full_vao ? full_VAO : VAO);
    
    // Draw a small cube for each hot point
    float spacing = 0.5f;
    float offset = (part - 1) * spacing / 2.0f;
    
    for (int i = 0; i < part; i++) {
        for (int j = 0; j < part; j++) {
            for (int k = 0; k < part; k++) {
                if (mat[i][j][k] > 0.5) {
                    float x = k * spacing - offset;
                    float y = j * spacing - offset;
                    float z = i * spacing - offset;
                    
                    glVertexAttrib3f(1, x, y, z);  // offset
                    glVertexAttrib1f(2, mat[i][j][k]);  // temp
                    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                }
            }
        }
    }
    
    glfwSwapBuffers(target_window);
}

void renderFullCube(data_type**** all_mats, int part, int num_ranks) {
    if (!full_window) return;
    
    glfwMakeContextCurrent(full_window);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(shaderProgram);
    
    // View matrix - simple camera for full cube
    float rad_x = full_camera_angle_x * 3.14159f / 180.0f;
    float rad_y = full_camera_angle_y * 3.14159f / 180.0f;
    float cam_x = full_camera_distance * cosf(rad_x) * sinf(rad_y);
    float cam_y = full_camera_distance * sinf(rad_x);
    float cam_z = full_camera_distance * cosf(rad_x) * cosf(rad_y);
    
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, -cam_x,-cam_y,-cam_z,1};
    
    float fov = 60.0f * 3.14159f / 180.0f;
    float aspect = (float)window_width / (float)window_height;
    float n = 0.1f, f = 100.0f;
    float fd = 1.0f / tanf(fov / 2.0f);
    float projection[16] = {
        fd/aspect,0,0,0, 0,fd,0,0, 0,0,(f+n)/(n-f),-1, 0,0,(2*f*n)/(n-f),0
    };
    
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);
    
    glBindVertexArray(full_VAO);
    
    float spacing = 0.5f;
    float full_size = N + 2;
    float full_offset = (full_size - 1) * spacing / 2.0f;
    
    // Draw cubes from all ranks in their proper positions
    for (int rank = 0; rank < num_ranks; rank++) {
        int row = rank / 2;
        int col = rank % 2;
        int rank_offset_y = row * (part - 1);
        int rank_offset_x = col * (part - 1);
        
        for (int i = 0; i < part; i++) {
            for (int j = 0; j < part; j++) {
                for (int k = 0; k < part; k++) {
                    if (all_mats[rank][i][j][k] > 0.5) {
                        float x = (k + rank_offset_x) * spacing - full_offset;
                        float y = (j + rank_offset_y) * spacing - full_offset;
                        float z = i * spacing - full_offset;
                        
                        glVertexAttrib3f(1, x, y, z);
                        glVertexAttrib1f(2, all_mats[rank][i][j][k]);
                        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                    }
                }
            }
        }
    }
    
    glfwSwapBuffers(full_window);
}

void initialize(data_type*** mat, int rank, int size) {
    int part = ((N+2)/2)+1;
    
    // Initialize all to zero
    for (int i = 0; i < part; i++) {
        for (int j = 0; j < part; j++) {
            for (int k = 0; k < part; k++) {
                mat[i][j][k] = 0.;
            }
        }
    }
    
    // 3D domain decomposition: 2x2x2 = 8 ranks (or 2x2x1 for 4 ranks)
    // Rank layout: row = rank/4, layer = (rank%4)/2, col = rank%2
    int layer = (rank % 4) / 2;  // 0 or 1
    int col = rank % 2;           // 0 or 1
    
    // Only the FRONT layer (layer=0) gets the hot boundary
    if (layer == 0) {
        for (int i = 0; i < part; i++) {
            for (int j = 0; j < part; j++) {
                mat[i][j][0] = 100.;  // z=0 face is hot
            }
        }
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
}

void copy(data_type*** og, data_type*** cpy, int size){
    for(int i = 0; i < size; i++){
        for(int j = 0; j < size; j++){
            for(int k = 0; k < size; k++){
                cpy[i][j][k] = og[i][j][k];
            }
        }
    }
}

void simulation(data_type*** mat, int rank, int size, int visualize) {
    int part = ((N + 2)/2) + 1;
    
    // Calculate neighbor ranks for 2x2x2 decomposition
    int layer = (rank % 4) / 2;  // z direction: 0 or 1
    int row = (rank % 2);         // y direction: 0 or 1  
    int col = rank / 4;           // x direction: 0 or 1 (for 8 ranks)
    
    // For 4 ranks (2x2x1), simplify:
    if (size == 4) {
        row = rank / 2;    // y: 0 or 1
        col = rank % 2;    // x: 0 or 1
        layer = 0;         // z: all same layer
    }
    
    // Neighbor ranks (6 faces in 3D)
    int neigh_xp = (col == 0 && size > 4) ? rank + 4 : -1;  // x+
    int neigh_xm = (col == 1 && size > 4) ? rank - 4 : -1;  // x-
    int neigh_yp = (row == 0) ? rank + 2 : -1;              // y+
    int neigh_ym = (row == 1) ? rank - 2 : -1;              // y-
    int neigh_zp = (layer == 0 && size > 4) ? rank + 2 : -1; // z+
    int neigh_zm = (layer == 1 && size > 4) ? rank - 2 : -1; // z-
    
    data_type*** old = (data_type***)malloc(sizeof(data_type**)*part);
    for (int i = 0; i < part; i++) {
        old[i] = (data_type**)malloc(sizeof(data_type*)*part);
        for (int j = 0; j < part; j++) {
            old[i][j] = (data_type*)malloc(sizeof(data_type)*part);
        }
    }
    
    copy(mat, old, part);
    
    // Buffers for boundary exchange
    data_type *send_buf = (data_type*)malloc(part * part * sizeof(data_type));
    data_type *recv_buf = (data_type*)malloc(part * part * sizeof(data_type));
    MPI_Request requests[12];
    MPI_Status statuses[12];
    
    float global_eps = EPSILON + 1;
    int iteration = 0;
    int done = 0;
    
    if (rank == 0) {
        printf("\nStarting simulation with %d ranks...\n", size);
        printf("Grid size: %dx%dx%d\n", N, N, N);
        printf("Each rank has: %dx%dx%d cells\n", part, part, part);
        printf("Rank %d neighbors: x+=%d x-=%d y+=%d y-=%d z+=%d z-=%d\n\n", 
               rank, neigh_xp, neigh_xm, neigh_yp, neigh_ym, neigh_zp, neigh_zm);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    while (!done && (!visualize || !glfwWindowShouldClose(window))) {
        int req_count = 0;
        
        // ===== NON-BLOCKING BOUNDARY EXCHANGE =====
        
        // X+ direction (send right face, recv from right)
        if (neigh_xp >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    send_buf[i * part + j] = mat[i][j][part-2];
                }
            }
            MPI_Isend(send_buf, part*part, MPI_FLOAT, neigh_xp, 0, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_buf, part*part, MPI_FLOAT, neigh_xp, 1, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // X- direction (send left face, recv from left)
        if (neigh_xm >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    send_buf[i * part + j] = mat[i][j][1];
                }
            }
            MPI_Isend(send_buf, part*part, MPI_FLOAT, neigh_xm, 1, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_buf, part*part, MPI_FLOAT, neigh_xm, 0, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Y+ direction (send top face, recv from top)
        if (neigh_yp >= 0) {
            for (int i = 0; i < part; i++) {
                for (int k = 0; k < part; k++) {
                    send_buf[i * part + k] = mat[part-2][i][k];
                }
            }
            MPI_Isend(send_buf, part*part, MPI_FLOAT, neigh_yp, 2, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_buf, part*part, MPI_FLOAT, neigh_yp, 3, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Y- direction (send bottom face, recv from bottom)
        if (neigh_ym >= 0) {
            for (int i = 0; i < part; i++) {
                for (int k = 0; k < part; k++) {
                    send_buf[i * part + k] = mat[1][i][k];
                }
            }
            MPI_Isend(send_buf, part*part, MPI_FLOAT, neigh_ym, 3, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_buf, part*part, MPI_FLOAT, neigh_ym, 2, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Z+ direction (send back face, recv from back)
        if (neigh_zp >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    send_buf[i * part + j] = mat[i][j][part-2];
                }
            }
            MPI_Isend(send_buf, part*part, MPI_FLOAT, neigh_zp, 4, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_buf, part*part, MPI_FLOAT, neigh_zp, 5, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Z- direction (send front face, recv from front)
        if (neigh_zm >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    send_buf[i * part + j] = mat[i][j][1];
                }
            }
            MPI_Isend(send_buf, part*part, MPI_FLOAT, neigh_zm, 5, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(recv_buf, part*part, MPI_FLOAT, neigh_zm, 4, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Wait for all communications to complete
        if (req_count > 0) {
            MPI_Waitall(req_count, requests, statuses);
        }
        
        // Update ghost cells with received data
        req_count = 0;
        if (neigh_xp >= 0) {
            // Skip - already received, now unpack
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    mat[i][j][part-1] = recv_buf[i * part + j];
                }
            }
            req_count += 2;
        }
        if (neigh_xm >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    mat[i][j][0] = recv_buf[i * part + j];
                }
            }
            req_count += 2;
        }
        if (neigh_yp >= 0) {
            for (int i = 0; i < part; i++) {
                for (int k = 0; k < part; k++) {
                    mat[part-1][i][k] = recv_buf[i * part + k];
                }
            }
            req_count += 2;
        }
        if (neigh_ym >= 0) {
            for (int i = 0; i < part; i++) {
                for (int k = 0; k < part; k++) {
                    mat[0][i][k] = recv_buf[i * part + k];
                }
            }
            req_count += 2;
        }
        if (neigh_zp >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    mat[i][j][part-1] = recv_buf[i * part + j];
                }
            }
            req_count += 2;
        }
        if (neigh_zm >= 0) {
            for (int i = 0; i < part; i++) {
                for (int j = 0; j < part; j++) {
                    mat[i][j][0] = recv_buf[i * part + j];
                }
            }
            req_count += 2;
        }
        
        // ===== COMPUTE HEAT DIFFUSION =====
        float max_eps = 0.0;
        
        for (int i = 1; i < part-1; i++) {
            for (int j = 1; j < part-1; j++) {
                for (int k = 1; k < part-1; k++) {
                    float delta = ALPHA * (
                        old[i+1][j][k] + old[i-1][j][k] +
                        old[i][j+1][k] + old[i][j-1][k] +
                        old[i][j][k+1] + old[i][j][k-1] -
                        6 * old[i][j][k]
                    );
                    
                    mat[i][j][k] += delta;
                    float eps = fabsf(delta / (mat[i][j][k] + 0.001f));
                    if (eps > max_eps) max_eps = eps;
                }
            }
        }
        
        copy(mat, old, part);
        MPI_Allreduce(&max_eps, &global_eps, 1, MPI_FLOAT, MPI_MAX, MPI_COMM_WORLD);
        
        if (global_eps <= EPSILON) done = 1;
        
        // Visualize
        if (visualize && (done || iteration % 2 == 0)) {
            processInput(window);
            renderCubes(mat, part, window, 0, camera_angle_x, camera_angle_y, camera_distance);
            
            // Collect and render full cube on rank 0
            if (rank == 0) {
                // Allocate storage for all ranks' data
                data_type**** all_mats = (data_type****)malloc(sizeof(data_type***) * size);
                all_mats[0] = mat;  // Rank 0's own data
                
                // Receive from other ranks
                for (int r = 1; r < size; r++) {
                    all_mats[r] = (data_type***)malloc(sizeof(data_type**)*part);
                    for (int i = 0; i < part; i++) {
                        all_mats[r][i] = (data_type**)malloc(sizeof(data_type*)*part);
                        for (int j = 0; j < part; j++) {
                            all_mats[r][i][j] = (data_type*)malloc(sizeof(data_type)*part);
                        }
                    }
                    
                    // Receive flattened data
                    data_type* recv_buffer = (data_type*)malloc(part*part*part*sizeof(data_type));
                    MPI_Recv(recv_buffer, part*part*part, MPI_FLOAT, r, 99, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    
                    // Unflatten
                    for (int i = 0; i < part; i++) {
                        for (int j = 0; j < part; j++) {
                            for (int k = 0; k < part; k++) {
                                all_mats[r][i][j][k] = recv_buffer[i*part*part + j*part + k];
                            }
                        }
                    }
                    free(recv_buffer);
                }
                
                // Render full cube
                processInput(full_window);
                renderFullCube(all_mats, part, size);
                
                // Free received data
                for (int r = 1; r < size; r++) {
                    for (int i = 0; i < part; i++) {
                        for (int j = 0; j < part; j++) {
                            free(all_mats[r][i][j]);
                        }
                        free(all_mats[r][i]);
                    }
                    free(all_mats[r]);
                }
                free(all_mats);
                
            } else {
                // Send data to rank 0
                data_type* send_buffer = (data_type*)malloc(part*part*part*sizeof(data_type));
                for (int i = 0; i < part; i++) {
                    for (int j = 0; j < part; j++) {
                        for (int k = 0; k < part; k++) {
                            send_buffer[i*part*part + j*part + k] = mat[i][j][k];
                        }
                    }
                }
                MPI_Send(send_buffer, part*part*part, MPI_FLOAT, 0, 99, MPI_COMM_WORLD);
                free(send_buffer);
            }
            
            glfwPollEvents();
            
            if (rank == 0 && iteration % 100 == 0) {
                printf("Iteration %d, eps: %.6f\n", iteration, global_eps);
            }
        }
        
        iteration++;
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    if (rank == 0) {
        printf("\n✓ Simulation converged after %d iterations!\n", iteration);
        printf("Final epsilon: %.6f\n\n", global_eps);
        printf("Controls:\n");
        printf("  • Left-click + drag to rotate\n");
        printf("  • Scroll to zoom\n");
        printf("  • ESC to close\n\n");
    }
    
    // Keep rendering after simulation
    if (visualize) {
        while (!glfwWindowShouldClose(window) && (!full_window || !glfwWindowShouldClose(full_window))) {
            processInput(window);
            renderCubes(mat, part, window, 0, camera_angle_x, camera_angle_y, camera_distance);
            
            // Update full cube view
            if (rank == 0 && full_window) {
                data_type**** all_mats = (data_type****)malloc(sizeof(data_type***) * size);
                all_mats[0] = mat;
                
                for (int r = 1; r < size; r++) {
                    all_mats[r] = (data_type***)malloc(sizeof(data_type**)*part);
                    for (int i = 0; i < part; i++) {
                        all_mats[r][i] = (data_type**)malloc(sizeof(data_type*)*part);
                        for (int j = 0; j < part; j++) {
                            all_mats[r][i][j] = (data_type*)malloc(sizeof(data_type)*part);
                        }
                    }
                    
                    data_type* recv_buffer = (data_type*)malloc(part*part*part*sizeof(data_type));
                    MPI_Recv(recv_buffer, part*part*part, MPI_FLOAT, r, 99, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    
                    for (int i = 0; i < part; i++) {
                        for (int j = 0; j < part; j++) {
                            for (int k = 0; k < part; k++) {
                                all_mats[r][i][j][k] = recv_buffer[i*part*part + j*part + k];
                            }
                        }
                    }
                    free(recv_buffer);
                }
                
                processInput(full_window);
                renderFullCube(all_mats, part, size);
                
                for (int r = 1; r < size; r++) {
                    for (int i = 0; i < part; i++) {
                        for (int j = 0; j < part; j++) {
                            free(all_mats[r][i][j]);
                        }
                        free(all_mats[r][i]);
                    }
                    free(all_mats[r]);
                }
                free(all_mats);
            } else {
                data_type* send_buffer = (data_type*)malloc(part*part*part*sizeof(data_type));
                for (int i = 0; i < part; i++) {
                    for (int j = 0; j < part; j++) {
                        for (int k = 0; k < part; k++) {
                            send_buffer[i*part*part + j*part + k] = mat[i][j][k];
                        }
                    }
                }
                MPI_Send(send_buffer, part*part*part, MPI_FLOAT, 0, 99, MPI_COMM_WORLD);
                free(send_buffer);
            }
            
            glfwPollEvents();
            
            int should_close = glfwWindowShouldClose(window) || (full_window && glfwWindowShouldClose(full_window));
            int global_close = 0;
            MPI_Allreduce(&should_close, &global_close, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
            if (global_close) break;
        }
    }
    
    // Cleanup
    free(send_buf);
    free(recv_buf);
    for (int i = 0; i < part; i++) {
        for (int j = 0; j < part; j++) {
            free(old[i][j]);
        }
        free(old[i]);
    }
    free(old);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int visualize = (argc > 1 && strcmp(argv[1], "--visualize") == 0);

    int part = ((N + 2)/2) + 1;
    
    data_type ***mat = (data_type***)malloc(sizeof(data_type**)*part);
    for (int i = 0; i < part; i++) {
        mat[i] = (data_type**)malloc(sizeof(data_type*)*part);
        for (int j = 0; j < part; j++) {
            mat[i][j] = (data_type*)malloc(sizeof(data_type)*part);
        }
    }
    
    initialize(mat, world_rank, world_size);
    
    if (visualize) {
        if (initOpenGL(world_rank) == 0) {
            setupCubeBuffers();
        } else {
            visualize = 0;
        }
    }
    
    simulation(mat, world_rank, world_size, visualize);
    
    if (visualize) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glfwTerminate();
    }
    
    for (int i = 0; i < part; i++) {
        for (int j = 0; j < part; j++) {
            free(mat[i][j]);
        }
        free(mat[i]);
    }
    free(mat);

    MPI_Finalize();
    return 0;
}