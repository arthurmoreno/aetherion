#include "Gui/Gui.hpp"

// Forward declarations for helper methods
void renderVoxelDataHeader(nb::ndarray<nb::numpy>& voxel_data);
void renderTransformControls(float translation[3], float rotation[3], float scale[3], 
                           float& viewDistance, float& zoom, bool& matrixChanged,
                           float cameraPosition[3], float cameraTarget[3], float cameraUp[3], bool& cameraChanged);
void renderImGuizmoControls(ImGuizmo::OPERATION& currentGizmoOperation, ImGuizmo::MODE& currentGizmoMode, 
                          bool& useSnap, float snap[3], bool& showGrid, bool& showAxes, bool& showWireframe, bool& showImGuizmo, bool& showVoxelBorders, bool& showDebugFaceOrder);
void updateTransformationMatrix(float objectMatrix[16], const float translation[3], 
                              const float rotation[3], const float scale[3], bool matrixChanged);
void setupProjectionMatrix(float cameraProjection[16], float aspect);
void render3DViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data, 
                     float cameraView[16], float cameraProjection[16], float objectMatrix[16],
                     ImGuizmo::OPERATION currentGizmoOperation, ImGuizmo::MODE currentGizmoMode,
                     bool useSnap, float snap[3], float zoom, float viewDistance,
                     bool showGrid, bool showAxes, bool showWireframe, bool showImGuizmo, bool showVoxelBorders, bool showDebugFaceOrder);

// Main function to render the 3D voxel viewport in a single organized window
void render3DVoxelViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data) {
    // Create single main window
    ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("3D Voxel Viewport", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Static camera and transformation matrices
    static float cameraView[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, -5.0f, 1.0f
    };
    
    static float cameraProjection[16];
    static float objectMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    // Static control variables
    static float translation[3] = { -0.3f, -0.3f, 5.0f };
    static float rotation[3] = { -45.0f, 45.0f, -90.0f };
    static float scale[3] = { 1.0f, 1.0f, 1.0f };
    static float viewDistance = 25.0f;
    static float zoom = 20.0f;
    static bool matrixChanged = true;
    
    // Camera position controls
    static float cameraPosition[3] = { 15.0f, 15.0f, 15.0f };
    static float cameraTarget[3] = { 8.0f, 8.0f, 8.0f };
    static float cameraUp[3] = { 0.0f, 1.0f, 0.0f };
    static bool cameraChanged = true;
    
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;
    static bool useSnap = false;
    static float snap[3] = { 1.0f, 1.0f, 1.0f };
    
    // Static visibility flags
    static bool showGrid = false;
    static bool showAxes = true;
    static bool showWireframe = true;
    static bool showImGuizmo = false;
    static bool showVoxelBorders = true;
    static bool showDebugFaceOrder = false;
    
    // Layout control variables
    static bool showControlPanel = true;
    static float controlPanelWidth = 350.0f;
    
    // Menu bar for layout options
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Show Control Panel", &showControlPanel);
            ImGui::SliderFloat("Control Panel Width", &controlPanelWidth, 250.0f, 500.0f);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                controlPanelWidth = 350.0f;
                showControlPanel = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Reset Camera")) {
                cameraPosition[0] = 15.0f; cameraPosition[1] = 15.0f; cameraPosition[2] = 15.0f;
                cameraTarget[0] = 8.0f; cameraTarget[1] = 8.0f; cameraTarget[2] = 8.0f;
                cameraUp[0] = 0.0f; cameraUp[1] = 1.0f; cameraUp[2] = 0.0f;
                cameraChanged = true;
            }
            if (ImGui::MenuItem("Tibia View")) {
                cameraPosition[0] = 20.0f; cameraPosition[1] = 20.0f; cameraPosition[2] = 20.0f;
                cameraTarget[0] = 8.0f; cameraTarget[1] = 8.0f; cameraTarget[2] = 8.0f;
                cameraUp[0] = 0.0f; cameraUp[1] = 1.0f; cameraUp[2] = 0.0f;
                cameraChanged = true;
            }
            if (ImGui::MenuItem("Top View")) {
                cameraPosition[0] = 8.0f; cameraPosition[1] = 30.0f; cameraPosition[2] = 8.0f;
                cameraTarget[0] = 8.0f; cameraTarget[1] = 0.0f; cameraTarget[2] = 8.0f;
                cameraUp[0] = 0.0f; cameraUp[1] = 0.0f; cameraUp[2] = -1.0f;
                cameraChanged = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Update transformation matrix
    updateTransformationMatrix(objectMatrix, translation, rotation, scale, matrixChanged);
    
    // Dynamic camera view matrix calculation
    if (cameraChanged || matrixChanged) {
        // Calculate camera view matrix from position, target, and up vectors
        float forward[3] = {
            cameraTarget[0] - cameraPosition[0],
            cameraTarget[1] - cameraPosition[1],
            cameraTarget[2] - cameraPosition[2]
        };
        
        // Normalize forward vector
        float forwardLen = sqrtf(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
        if (forwardLen > 0.0f) {
            forward[0] /= forwardLen;
            forward[1] /= forwardLen;
            forward[2] /= forwardLen;
        }
        
        // Calculate right vector (cross product of forward and up)
        float right[3] = {
            forward[1] * cameraUp[2] - forward[2] * cameraUp[1],
            forward[2] * cameraUp[0] - forward[0] * cameraUp[2],
            forward[0] * cameraUp[1] - forward[1] * cameraUp[0]
        };
        
        // Normalize right vector
        float rightLen = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        if (rightLen > 0.0f) {
            right[0] /= rightLen;
            right[1] /= rightLen;
            right[2] /= rightLen;
        }
        
        // Calculate true up vector (cross product of right and forward)
        float up[3] = {
            right[1] * forward[2] - right[2] * forward[1],
            right[2] * forward[0] - right[0] * forward[2],
            right[0] * forward[1] - right[1] * forward[0]
        };
        
        // Build view matrix (column-major)
        cameraView[0] = right[0];   cameraView[4] = right[1];   cameraView[8] = right[2];    cameraView[12] = -(right[0]*cameraPosition[0] + right[1]*cameraPosition[1] + right[2]*cameraPosition[2]);
        cameraView[1] = up[0];      cameraView[5] = up[1];      cameraView[9] = up[2];       cameraView[13] = -(up[0]*cameraPosition[0] + up[1]*cameraPosition[1] + up[2]*cameraPosition[2]);
        cameraView[2] = -forward[0]; cameraView[6] = -forward[1]; cameraView[10] = -forward[2]; cameraView[14] = -(-forward[0]*cameraPosition[0] + -forward[1]*cameraPosition[1] + -forward[2]*cameraPosition[2]);
        cameraView[3] = 0.0f;       cameraView[7] = 0.0f;       cameraView[11] = 0.0f;       cameraView[15] = 1.0f;
        
        cameraChanged = false;
    }
    
    // Main layout: Control panel (left) + 3D Viewport (right)
    ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    
    if (showControlPanel) {
        // Left panel - Tabbed control interface
        ImGui::BeginChild("ControlPanel", ImVec2(controlPanelWidth, availableRegion.y), true);
        
        if (ImGui::BeginTabBar("ControlTabs", ImGuiTabBarFlags_None)) {
            
            // Tab 1: Voxel Info
            if (ImGui::BeginTabItem("Info")) {
                renderVoxelDataHeader(voxel_data);
                ImGui::EndTabItem();
            }
            
            // Tab 2: Transform & Camera
            if (ImGui::BeginTabItem("Transform")) {
                // Collapsible sections for better organization
                if (ImGui::CollapsingHeader("Transform Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::SliderFloat3("Translation", translation, -50.0f, 50.0f)) {
                        matrixChanged = true;
                    }
                    if (ImGui::SliderFloat3("Rotation (deg)", rotation, -180.0f, 180.0f)) {
                        matrixChanged = true;
                    }
                    if (ImGui::SliderFloat3("Scale", scale, 0.1f, 3.0f)) {
                        matrixChanged = true;
                    }
                    if (ImGui::SliderFloat("View Distance", &viewDistance, 1.0f, 20.0f)) {
                        matrixChanged = true;
                    }
                    if (ImGui::SliderFloat("Zoom", &zoom, 0.1f, 300.0f)) {
                        matrixChanged = true;
                    }
                    
                    if (ImGui::Button("Reset Transform")) {
                        translation[0] = -0.3f; translation[1] = -0.3f; translation[2] = 5.0f;
                        rotation[0] = -45.0f; rotation[1] = 45.0f; rotation[2] = -90.0f;
                        scale[0] = scale[1] = scale[2] = 1.0f;
                        viewDistance = 25.0f; zoom = 20.0f;
                        matrixChanged = true;
                    }
                }
                
                if (ImGui::CollapsingHeader("Camera Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::SliderFloat3("Camera Position", cameraPosition, -50.0f, 50.0f)) {
                        cameraChanged = true;
                    }
                    if (ImGui::SliderFloat3("Camera Target", cameraTarget, -20.0f, 20.0f)) {
                        cameraChanged = true;
                    }
                    if (ImGui::SliderFloat3("Camera Up", cameraUp, -1.0f, 1.0f)) {
                        cameraChanged = true;
                    }
                }
                
                ImGui::EndTabItem();
            }
            
            // Tab 3: Display Options
            if (ImGui::BeginTabItem("Display")) {
                if (ImGui::CollapsingHeader("ImGuizmo Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE))
                        currentGizmoOperation = ImGuizmo::TRANSLATE;
                    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE))
                        currentGizmoOperation = ImGuizmo::ROTATE;
                    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE))
                        currentGizmoOperation = ImGuizmo::SCALE;
                    
                    if (currentGizmoOperation != ImGuizmo::SCALE) {
                        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL))
                            currentGizmoMode = ImGuizmo::LOCAL;
                        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD))
                            currentGizmoMode = ImGuizmo::WORLD;
                    }
                    
                    ImGui::Checkbox("Use Snap", &useSnap);
                    if (useSnap) {
                        ImGui::InputFloat3("Snap Values", snap);
                    }
                }
                
                if (ImGui::CollapsingHeader("Visibility Options", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Show Grid", &showGrid);
                    ImGui::Checkbox("Show Axes", &showAxes);
                    ImGui::Checkbox("Show Wireframe", &showWireframe);
                    ImGui::Checkbox("Show ImGuizmo", &showImGuizmo);
                    ImGui::Checkbox("Show Voxel Borders", &showVoxelBorders);
                    ImGui::Checkbox("Debug Face Order", &showDebugFaceOrder);
                }
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::EndChild();
        
        ImGui::SameLine();
    }
    
    // Right panel - 3D Viewport
    float viewportWidth = showControlPanel ? (availableRegion.x - controlPanelWidth - 10.0f) : availableRegion.x;
    ImGui::BeginChild("Viewport3D", ImVec2(viewportWidth, availableRegion.y), true);
    
    // Calculate aspect ratio and setup projection
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    float aspect = viewportSize.x / viewportSize.y;
    setupProjectionMatrix(cameraProjection, aspect);
    
    // Render the 3D viewport
    render3DViewport(voxel_data, shared_data, cameraView, cameraProjection, objectMatrix,
                    currentGizmoOperation, currentGizmoMode, useSnap, snap, zoom, viewDistance,
                    showGrid, showAxes, showWireframe, showImGuizmo, showVoxelBorders, showDebugFaceOrder);
    
    ImGui::EndChild();
    
    ImGui::End();
}

// Render the header with basic voxel data information
void renderVoxelDataHeader(nb::ndarray<nb::numpy>& voxel_data) {
    ImGui::Text("Voxel Data Information");
    ImGui::Separator();
    
    // Display dimensions and shape in a row
    ImGui::Text("Dimensions: %zu", voxel_data.ndim());
    ImGui::SameLine();
    
    // Display shape information
    std::string shapeStr = "Shape: [";
    for (size_t i = 0; i < voxel_data.ndim(); i++) {
        shapeStr += std::to_string(voxel_data.shape(i));
        if (i < voxel_data.ndim() - 1) shapeStr += ", ";
    }
    shapeStr += "]";
    ImGui::Text("%s", shapeStr.c_str());
    ImGui::SameLine();
    
    // Display total size
    size_t totalSize = 1;
    for (size_t i = 0; i < voxel_data.ndim(); i++) {
        totalSize *= voxel_data.shape(i);
    }
    ImGui::Text("Total elements: %zu", totalSize);
    
    // Display sample data values if available
    if (totalSize > 0 && voxel_data.data() != nullptr) {
        if (voxel_data.dtype() == nb::dtype<float>()) {
            const float* data = static_cast<const float*>(voxel_data.data());
            ImGui::Text("Sample values (float): %.3f, %.3f, %.3f", 
                totalSize > 0 ? data[0] : 0.0f,
                totalSize > 1 ? data[1] : 0.0f,
                totalSize > 2 ? data[2] : 0.0f);
        } else if (voxel_data.dtype() == nb::dtype<int>()) {
            const int* data = static_cast<const int*>(voxel_data.data());
            ImGui::Text("Sample values (int): %d, %d, %d", 
                totalSize > 0 ? data[0] : 0,
                totalSize > 1 ? data[1] : 0,
                totalSize > 2 ? data[2] : 0);
        } else {
            ImGui::Text("Data type: Unsupported for preview");
        }
    }
}

// Render transformation control sliders
void renderTransformControls(float translation[3], float rotation[3], float scale[3], 
                           float& viewDistance, float& zoom, bool& matrixChanged,
                           float cameraPosition[3], float cameraTarget[3], float cameraUp[3], bool& cameraChanged) {
    ImGui::Text("Transform Controls");
    ImGui::Separator();
    
    if (ImGui::SliderFloat3("Translation", translation, -50.0f, 50.0f)) {
        matrixChanged = true;
    }
    if (ImGui::SliderFloat3("Rotation (deg)", rotation, -180.0f, 180.0f)) {
        matrixChanged = true;
    }
    if (ImGui::SliderFloat3("Scale", scale, 0.1f, 3.0f)) {
        matrixChanged = true;
    }
    if (ImGui::SliderFloat("View Distance", &viewDistance, 1.0f, 20.0f)) {
        matrixChanged = true;
    }
    if (ImGui::SliderFloat("Zoom", &zoom, 0.1f, 300.0f)) {
        matrixChanged = true;
    }
    
    ImGui::Separator();
    ImGui::Text("Camera Controls");
    
    if (ImGui::SliderFloat3("Camera Position", cameraPosition, -50.0f, 50.0f)) {
        cameraChanged = true;
    }
    if (ImGui::SliderFloat3("Camera Target", cameraTarget, -20.0f, 20.0f)) {
        cameraChanged = true;
    }
    if (ImGui::SliderFloat3("Camera Up", cameraUp, -1.0f, 1.0f)) {
        cameraChanged = true;
    }
    
    // Reset and preset buttons
    if (ImGui::Button("Reset Transform")) {
        translation[0] = -0.3f; // Slight offset to better view voxels
        translation[1] = -0.3f;
        translation[2] = 5.0f;
        rotation[0] = -45.0f;  // Tilt down to see the top and sides
        rotation[1] = 45.0f;   // Rotate 45 degrees to get isometric view
        rotation[2] = -90.0f;  // Roll
        scale[0] = scale[1] = scale[2] = 1.0f;
        viewDistance = 25.0f;
        zoom = 20.0f;
        matrixChanged = true;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) {
        cameraPosition[0] = 0.0f; cameraPosition[1] = 0.0f; cameraPosition[2] = -25.0f;
        cameraTarget[0] = 8.0f; cameraTarget[1] = 8.0f; cameraTarget[2] = 8.0f;  // Center of typical voxel data
        cameraUp[0] = 0.0f; cameraUp[1] = 1.0f; cameraUp[2] = 0.0f;
        cameraChanged = true;
    }
    
    if (ImGui::Button("Tibia View")) {
        // Set up classic isometric Tibia-like perspective
        translation[0] = translation[1] = translation[2] = 0.0f;
        rotation[0] = -45.0f;  // Tilt down to see the top and sides
        rotation[1] = -45.0f;  // Rotate 45 degrees to get isometric view
        rotation[2] = 0.0f;    // No roll
        scale[0] = scale[1] = scale[2] = 1.0f;
        viewDistance = 30.0f;  // Pull back for good view of voxel area
        zoom = 3.0f;           // Zoom in to see voxel details
        matrixChanged = true;
        
        // Position camera for isometric view
        cameraPosition[0] = 20.0f; cameraPosition[1] = 20.0f; cameraPosition[2] = 20.0f;
        cameraTarget[0] = 8.0f; cameraTarget[1] = 8.0f; cameraTarget[2] = 8.0f;
        cameraUp[0] = 0.0f; cameraUp[1] = 1.0f; cameraUp[2] = 0.0f;
        cameraChanged = true;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Top View")) {
        // Camera looking down from above
        cameraPosition[0] = 8.0f; cameraPosition[1] = 30.0f; cameraPosition[2] = 8.0f;
        cameraTarget[0] = 8.0f; cameraTarget[1] = 0.0f; cameraTarget[2] = 8.0f;
        cameraUp[0] = 0.0f; cameraUp[1] = 0.0f; cameraUp[2] = -1.0f;  // Z points up when looking down
        cameraChanged = true;
    }
    
    if (ImGui::Button("Random Rotation")) {
        rotation[0] = (rand() % 360) - 180.0f;
        rotation[1] = (rand() % 360) - 180.0f;
        rotation[2] = (rand() % 360) - 180.0f;
        matrixChanged = true;
    }
}

// Render ImGuizmo control panel
void renderImGuizmoControls(ImGuizmo::OPERATION& currentGizmoOperation, ImGuizmo::MODE& currentGizmoMode, 
                          bool& useSnap, float snap[3], bool& showGrid, bool& showAxes, bool& showWireframe, bool& showImGuizmo, bool& showVoxelBorders, bool& showDebugFaceOrder) {
    ImGui::Text("ImGuizmo Controls");
    ImGui::Separator();
    
    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE))
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE))
        currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE))
        currentGizmoOperation = ImGuizmo::SCALE;
    
    if (currentGizmoOperation != ImGuizmo::SCALE) {
        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL))
            currentGizmoMode = ImGuizmo::LOCAL;
        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD))
            currentGizmoMode = ImGuizmo::WORLD;
    }
    
    ImGui::Checkbox("Use Snap", &useSnap);
    if (useSnap) {
        switch (currentGizmoOperation) {
            case ImGuizmo::TRANSLATE:
                ImGui::InputFloat3("Snap", &snap[0]);
                break;
            case ImGuizmo::ROTATE:
                ImGui::InputFloat("Angle Snap", &snap[0]);
                break;
            case ImGuizmo::SCALE:
                ImGui::InputFloat("Scale Snap", &snap[0]);
                break;
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Display Options");
    
    // Grid visibility controls - now using references to the actual static variables
    ImGui::Checkbox("Show Grid", &showGrid);
    ImGui::Checkbox("Show Axes", &showAxes);  
    ImGui::Checkbox("Show Wireframe", &showWireframe);
    
    // Add option to show/hide ImGuizmo
    ImGui::Checkbox("Show ImGuizmo", &showImGuizmo);
    
    // Add option to show/hide voxel borders
    ImGui::Checkbox("Show Voxel Borders", &showVoxelBorders);
    
    // Add debug option to show face rendering order
    ImGui::Checkbox("Debug Face Order", &showDebugFaceOrder);
}

// Update the transformation matrix based on current control values
void updateTransformationMatrix(float objectMatrix[16], const float translation[3], 
                              const float rotation[3], const float scale[3], bool matrixChanged) {
    if (!matrixChanged) return;
    
    // Convert degrees to radians
    float rx = rotation[0] * 3.14159f / 180.0f;
    float ry = rotation[1] * 3.14159f / 180.0f;
    float rz = rotation[2] * 3.14159f / 180.0f;
    
    // Create individual rotation matrices (column-major, right-handed)
    float cos_x = cosf(rx), sin_x = sinf(rx);
    float cos_y = cosf(ry), sin_y = sinf(ry);
    float cos_z = cosf(rz), sin_z = sinf(rz);
    
    // Rotation around X axis
    float Rx[16] = {
        1, 0, 0, 0,
        0, cos_x, sin_x, 0,
        0, -sin_x, cos_x, 0,
        0, 0, 0, 1
    };
    
    // Rotation around Y axis
    float Ry[16] = {
        cos_y, 0, -sin_y, 0,
        0, 1, 0, 0,
        sin_y, 0, cos_y, 0,
        0, 0, 0, 1
    };
    
    // Rotation around Z axis
    float Rz[16] = {
        cos_z, sin_z, 0, 0,
        -sin_z, cos_z, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    // Scale matrix
    float S[16] = {
        scale[0], 0, 0, 0,
        0, scale[1], 0, 0,
        0, 0, scale[2], 0,
        0, 0, 0, 1
    };
    
    // Translation matrix
    float T[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        translation[0], translation[1], translation[2], 1
    };
    
    // Helper function to multiply 4x4 matrices (column-major)
    auto multiplyMatrix = [](const float* a, const float* b, float* result) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result[j * 4 + i] = 0;
                for (int k = 0; k < 4; k++) {
                    result[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
                }
            }
        }
    };
    
    // Compute M = T * Rz * Ry * Rx * S (proper composition order)
    float temp1[16], temp2[16], temp3[16], temp4[16];
    
    multiplyMatrix(Rx, S, temp1);     // Rx * S
    multiplyMatrix(Ry, temp1, temp2); // Ry * (Rx * S)
    multiplyMatrix(Rz, temp2, temp3); // Rz * (Ry * (Rx * S))
    multiplyMatrix(T, temp3, objectMatrix); // T * (Rz * (Ry * (Rx * S)))
}

// Setup the perspective projection matrix
void setupProjectionMatrix(float cameraProjection[16], float aspect) {
    const float fov = 45.0f * 3.14159f / 180.0f; // 45 degrees in radians
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;
    
    // Create perspective projection matrix manually
    float f = 1.0f / tanf(fov / 2.0f);
    cameraProjection[0] = f / aspect;
    cameraProjection[1] = 0.0f;
    cameraProjection[2] = 0.0f;
    cameraProjection[3] = 0.0f;
    
    cameraProjection[4] = 0.0f;
    cameraProjection[5] = f;
    cameraProjection[6] = 0.0f;
    cameraProjection[7] = 0.0f;
    
    cameraProjection[8] = 0.0f;
    cameraProjection[9] = 0.0f;
    cameraProjection[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    cameraProjection[11] = -1.0f;
    
    cameraProjection[12] = 0.0f;
    cameraProjection[13] = 0.0f;
    cameraProjection[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    cameraProjection[15] = 0.0f;
}

// Forward declarations for helper structs and functions
struct VoxelPoint {
    float x, y, z;
    int value;
    float depth;
};

void renderVoxelData(nb::ndarray<nb::numpy>& voxel_data, ImDrawList* drawList,
                    std::function<ImVec2(float, float, float)> projectToScreen,
                    std::function<std::tuple<float, float, float>(float, float, float)> transformPoint,
                    float zoom, size_t totalSize, bool showVoxelBorders, bool showDebugFaceOrder,
                    std::function<float(float, float, float)> cameraDepthFn);
void processFloatVoxelData(const float* data, nb::ndarray<nb::numpy>& voxel_data, 
                          std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize);
void processIntVoxelData(const int* data, nb::ndarray<nb::numpy>& voxel_data,
                        std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize);
void drawVoxelPoints(const std::vector<VoxelPoint>& voxelPoints, ImDrawList* drawList,
                    std::function<ImVec2(float, float, float)> projectToScreen,
                    std::function<std::tuple<float, float, float>(float, float, float)> transformPoint,
                    float zoom, nb::ndarray<nb::numpy>& voxel_data, bool showVoxelBorders, bool showDebugFaceOrder,
                    std::function<float(float, float, float)> cameraDepthFn);
void renderTransformationInfo(nb::ndarray<nb::numpy>& voxel_data, float objectMatrix[16]);
void drawCoordinateAxes(ImDrawList* drawList, std::function<ImVec2(float, float, float)> projectToScreen);
void drawGrid(ImDrawList* drawList, std::function<ImVec2(float, float, float)> projectToScreen, float zoom);
void drawUnitMeasurements(ImDrawList* drawList, std::function<ImVec2(float, float, float)> projectToScreen, float zoom);

// Render the main 3D viewport with voxels and ImGuizmo
void render3DViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data, 
                     float cameraView[16], float cameraProjection[16], float objectMatrix[16],
                     ImGuizmo::OPERATION currentGizmoOperation, ImGuizmo::MODE currentGizmoMode,
                     bool useSnap, float snap[3], float zoom, float viewDistance,
                     bool showGrid, bool showAxes, bool showWireframe, bool showImGuizmo, bool showVoxelBorders, bool showDebugFaceOrder) {
    
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    
    // Set ImGuizmo viewport
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(contentPos.x, contentPos.y, viewportSize.x, viewportSize.y);
    
    // Proper MVP (Model-View-Projection) transformation pipeline
    auto multiplyMatrix4 = [](const float* a, const float* b, float* result) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result[j * 4 + i] = 0;
                for (int k = 0; k < 4; k++) {
                    result[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
                }
            }
        }
    };
    
    // Compute MVP = Projection * View * Model and VM = View * Model
    float MV[16], MVP[16], VM[16];
    multiplyMatrix4(cameraView, objectMatrix, MV);      // MV = V * M
    multiplyMatrix4(cameraProjection, MV, MVP);         // MVP = P * (V * M)
    multiplyMatrix4(cameraView, objectMatrix, VM);      // VM = V * M (for depth calculation)
    
    // Camera depth function for proper depth sorting (camera-space z, not NDC)
    auto cameraDepth = [&](float x, float y, float z) -> float {
        return VM[2]*x + VM[6]*y + VM[10]*z + VM[14]; // camera-space z
    };
    
    // Transform point using proper MVP pipeline
    auto transformPoint = [&](float x, float y, float z) -> std::tuple<float, float, float> {
        // Apply MVP transformation (column vectors)
        float tx = MVP[0] * x + MVP[4] * y + MVP[8] * z + MVP[12];
        float ty = MVP[1] * x + MVP[5] * y + MVP[9] * z + MVP[13];
        float tz = MVP[2] * x + MVP[6] * y + MVP[10] * z + MVP[14];
        float tw = MVP[3] * x + MVP[7] * y + MVP[11] * z + MVP[15];
        
        // Perspective divide to get NDC coordinates
        if (tw != 0.0f) {
            tx /= tw;
            ty /= tw;
            tz /= tw;
        }
        
        return std::make_tuple(tx, ty, tz);
    };
    
    // Transform point for camera depth calculation (View * Model only)
    auto getCameraDepth = [&](float x, float y, float z) -> float {
        // Apply Model transformation first
        float mx = objectMatrix[0] * x + objectMatrix[4] * y + objectMatrix[8] * z + objectMatrix[12];
        float my = objectMatrix[1] * x + objectMatrix[5] * y + objectMatrix[9] * z + objectMatrix[13];
        float mz = objectMatrix[2] * x + objectMatrix[6] * y + objectMatrix[10] * z + objectMatrix[14];
        float mw = objectMatrix[3] * x + objectMatrix[7] * y + objectMatrix[11] * z + objectMatrix[15];
        
        if (mw != 0.0f) {
            mx /= mw; my /= mw; mz /= mw;
        }
        
        // Apply View transformation to get camera space depth
        float cz = cameraView[2] * mx + cameraView[6] * my + cameraView[10] * mz + cameraView[14];
        return cz; // Return camera space Z (depth)
    };
    
    // Project to screen coordinates using proper viewport transformation
    auto projectToScreen = [&](float x, float y, float z) -> ImVec2 {
        // Get NDC coordinates
        auto [ndcX, ndcY, ndcZ] = transformPoint(x, y, z);
        
        // Viewport transformation: NDC [-1,1] to screen coordinates
        float screenX = contentPos.x + (ndcX + 1.0f) * 0.5f * viewportSize.x;
        float screenY = contentPos.y + (1.0f - ndcY) * 0.5f * viewportSize.y; // Flip Y for screen coordinates
        
        return ImVec2(screenX, screenY);
    };
    
    // Render the 3D voxel visualization
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Draw coordinate axes and grid first (behind voxels) - conditionally
    if (showAxes) {
        drawCoordinateAxes(drawList, projectToScreen);
    }
    if (showGrid) {
        drawGrid(drawList, projectToScreen, zoom);
    }
    
    // Draw the voxel container outline using actual voxel data dimensions
    if (showWireframe) {
        // Get actual voxel data dimensions
        float width = 1.0f, height = 1.0f, depth = 1.0f;
        if (voxel_data.ndim() >= 2) {
            width = (float)voxel_data.shape(0);
            height = (float)voxel_data.shape(1);
            if (voxel_data.ndim() >= 3) {
                depth = (float)voxel_data.shape(2);
            }
        }
        
        // Define the 8 corners of the actual voxel data cube
        // Each voxel occupies 1 unit, so coordinates go from 0 to dimension
        std::vector<std::tuple<float, float, float>> cubeCorners = {
            {0.0f, 0.0f, 0.0f}, {width, 0.0f, 0.0f}, {width, height, 0.0f}, {0.0f, height, 0.0f},      // Back face
            {0.0f, 0.0f, depth}, {width, 0.0f, depth}, {width, height, depth}, {0.0f, height, depth}   // Front face
        };
        
        // Project all corners to screen space
        std::vector<ImVec2> screenCorners;
        for (const auto& corner : cubeCorners) {
            screenCorners.push_back(projectToScreen(std::get<0>(corner), std::get<1>(corner), std::get<2>(corner)));
        }
        
        // Draw the wireframe cube using the projected corners
        // Back face
        drawList->AddLine(screenCorners[0], screenCorners[1], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[1], screenCorners[2], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[2], screenCorners[3], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[3], screenCorners[0], IM_COL32(255, 255, 255, 255), 2.0f);
        
        // Front face
        drawList->AddLine(screenCorners[4], screenCorners[5], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[5], screenCorners[6], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[6], screenCorners[7], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[7], screenCorners[4], IM_COL32(255, 255, 255, 255), 2.0f);
        
        // Connecting edges
        drawList->AddLine(screenCorners[0], screenCorners[4], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[1], screenCorners[5], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[2], screenCorners[6], IM_COL32(255, 255, 255, 255), 2.0f);
        drawList->AddLine(screenCorners[3], screenCorners[7], IM_COL32(255, 255, 255, 255), 2.0f);
    }
    
    // Calculate total size for voxel data processing
    size_t totalSize = 1;
    for (size_t i = 0; i < voxel_data.ndim(); i++) {
        totalSize *= voxel_data.shape(i);
    }
    
    // Render voxels based on actual data
    if (totalSize > 0 && voxel_data.data() != nullptr) {
        renderVoxelData(voxel_data, drawList, projectToScreen, transformPoint, zoom, totalSize, showVoxelBorders, showDebugFaceOrder, cameraDepth);
    }
    
    // Use ImGuizmo to manipulate the object (only if enabled)
    if (showImGuizmo) {
        // Only draw the ImGuizmo manipulator when actively transforming, not the axes
        ImGuizmo::Enable(true);
        ImGuizmo::AllowAxisFlip(false);
        
        if (ImGuizmo::Manipulate(cameraView, cameraProjection, currentGizmoOperation, currentGizmoMode, 
                                objectMatrix, NULL, useSnap ? &snap[0] : NULL)) {
            // Store the transformation in shared_data for external access
            try {
                nb::list transformMatrix;
                for (int i = 0; i < 16; i++) {
                    transformMatrix.append(objectMatrix[i]);
                }
                shared_data["voxel_transform_matrix"] = transformMatrix;
            } catch (const std::exception& e) {
                std::cerr << "Error storing transformation matrix: " << e.what() << std::endl;
            }
        }
    }
    
    // Display transformation info and voxel data summary at the bottom
    renderTransformationInfo(voxel_data, objectMatrix);
}

// Render voxel data points
void renderVoxelData(nb::ndarray<nb::numpy>& voxel_data, ImDrawList* drawList,
                    std::function<ImVec2(float, float, float)> projectToScreen,
                    std::function<std::tuple<float, float, float>(float, float, float)> transformPoint,
                    float zoom, size_t totalSize, bool showVoxelBorders, bool showDebugFaceOrder,
                    std::function<float(float, float, float)> cameraDepthFn) {

    // Determine grid size based on data dimensions - support large voxel grids
    int gridSize = 64; // Default to support 64x64x64 voxel grids
    if (voxel_data.ndim() >= 2) {
        gridSize = std::max((int)voxel_data.shape(0), 64); // Use actual data size, minimum 64
    }

    // Collect all voxel positions for depth sorting
    std::vector<VoxelPoint> voxelPoints;
    
    // Render voxels based on data content
    if (voxel_data.dtype() == nb::dtype<float>()) {
        const float* data = static_cast<const float*>(voxel_data.data());
        processFloatVoxelData(data, voxel_data, voxelPoints, gridSize, totalSize);
    }
    // Handle integer data
    else if (voxel_data.dtype() == nb::dtype<int>()) {
        const int* data = static_cast<const int*>(voxel_data.data());
        processIntVoxelData(data, voxel_data, voxelPoints, gridSize, totalSize);
    }

    // Update depth values using proper camera-space depth (not NDC z)
    for (auto& voxel : voxelPoints) {
        float cx = voxel.x + 0.5f, cy = voxel.y + 0.5f, cz = voxel.z + 0.5f;
        voxel.depth = cameraDepthFn(cx, cy, cz); // camera-space z
    }

    // Sort voxels by camera depth (back to front for proper rendering)
    std::sort(voxelPoints.begin(), voxelPoints.end(), 
              [](const VoxelPoint& a, const VoxelPoint& b) { return a.depth < b.depth; });
    
    // Draw sorted voxels
    drawVoxelPoints(voxelPoints, drawList, projectToScreen, transformPoint, zoom, voxel_data, showVoxelBorders, showDebugFaceOrder, cameraDepthFn);
}

// Process float voxel data
void processFloatVoxelData(const float* data, nb::ndarray<nb::numpy>& voxel_data, 
                          std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize) {
    
    // For 3D data visualization
    if (voxel_data.ndim() >= 3) {
        size_t width = voxel_data.shape(0);
        size_t height = voxel_data.shape(1);
        size_t depth = voxel_data.shape(2);
        
        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t z = 0; z < depth; z++) {
                    size_t index = z * width * height + y * width + x;
                    if (index < totalSize) {
                        float value = data[index];
                        
                        // Only draw non-zero voxels
                        if (std::abs(value) > 0.001f) {
                            // Use actual spatial coordinates: each voxel = 1 unit
                            float nx = (float)x;
                            float ny = (float)y;
                            float nz = (float)z;
                            
                            // Use voxel center for depth calculation (will be updated with camera depth later)
                            voxelPoints.push_back({nx, ny, nz, (int)value, nz});
                        }
                    }
                }
            }
        }
    }
    // For 2D data visualization
    else if (voxel_data.ndim() >= 2) {
        size_t width = voxel_data.shape(0);
        size_t height = voxel_data.shape(1);
        
        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                size_t index = y * width + x;
                if (index < totalSize) {
                    float value = data[index];
                    
                    // Only draw non-zero voxels
                    if (std::abs(value) > 0.001f) {
                        // Use actual spatial coordinates, z=0 for 2D (at ground level)
                        float nx = (float)x;
                        float ny = (float)y;
                        float nz = 0.0f;
                        
                        voxelPoints.push_back({nx, ny, nz, (int)value, nz});
                    }
                }
            }
        }
    }
}

// Process integer voxel data  
void processIntVoxelData(const int* data, nb::ndarray<nb::numpy>& voxel_data,
                        std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize) {
    
    // For 3D data
    if (voxel_data.ndim() >= 3) {
        size_t width = voxel_data.shape(0);
        size_t height = voxel_data.shape(1);
        size_t depth = voxel_data.shape(2);
        
        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t z = 0; z < depth; z++) {
                    size_t index = z * width * height + y * width + x;
                    if (index < totalSize) {
                        int value = data[index];
                        
                        if (value != 0) {
                            // Use actual spatial coordinates: each voxel = 1 unit
                            // Place voxel at its grid position (no offset needed)
                            float nx = (float)x;
                            float ny = (float)y;
                            float nz = (float)z;
                            
                            voxelPoints.push_back({nx, ny, nz, value, nz});
                        }
                    }
                }
            }
        }
    }
    // For 2D data
    else if (voxel_data.ndim() >= 2) {
        size_t width = voxel_data.shape(0);
        size_t height = voxel_data.shape(1);

        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                size_t index = y * width + x;
                if (index < totalSize) {
                    int value = data[index];

                    if (value != 0) {
                        // Use actual spatial coordinates, z=0 for 2D (at ground level)
                        float nx = (float)x;
                        float ny = (float)y;
                        float nz = 0.0f;
                        
                        voxelPoints.push_back({nx, ny, nz, value, nz});
                    }
                }
            }
        }
    }
}

// Draw voxel points on screen
void drawVoxelPoints(const std::vector<VoxelPoint>& voxelPoints, ImDrawList* drawList,
                    std::function<ImVec2(float, float, float)> projectToScreen,
                    std::function<std::tuple<float, float, float>(float, float, float)> transformPoint,
                    float zoom, nb::ndarray<nb::numpy>& voxel_data, bool showVoxelBorders, bool showDebugFaceOrder,
                    std::function<float(float, float, float)> cameraDepthFn) {
    
    // Helper function to check if a voxel exists at given coordinates
    auto hasVoxelAt = [&](int x, int y, int z) -> bool {
        // Check bounds
        if (x < 0 || y < 0 || z < 0) return false;
        if (voxel_data.ndim() < 3) {
            if (voxel_data.ndim() == 2) {
                if (x >= (int)voxel_data.shape(0) || y >= (int)voxel_data.shape(1) || z > 0) return false;
            } else {
                return false;
            }
        } else {
            if (x >= (int)voxel_data.shape(0) || y >= (int)voxel_data.shape(1) || z >= (int)voxel_data.shape(2)) return false;
        }
        
        // Check if voxel has non-zero value
        if (voxel_data.dtype() == nb::dtype<float>()) {
            const float* data = static_cast<const float*>(voxel_data.data());
            size_t index;
            if (voxel_data.ndim() == 3) {
                index = z * voxel_data.shape(0) * voxel_data.shape(1) + y * voxel_data.shape(0) + x;
            } else {
                index = y * voxel_data.shape(0) + x;
            }
            return std::abs(data[index]) > 0.001f;
        } else if (voxel_data.dtype() == nb::dtype<int>()) {
            const int* data = static_cast<const int*>(voxel_data.data());
            size_t index;
            if (voxel_data.ndim() == 3) {
                index = z * voxel_data.shape(0) * voxel_data.shape(1) + y * voxel_data.shape(0) + x;
            } else {
                index = y * voxel_data.shape(0) + x;
            }
            return data[index] != 0;
        }
        return false;
    };
    
    for (const auto& voxel : voxelPoints) {
        // Each voxel occupies a 1x1x1 cube from (x,y,z) to (x+1,y+1,z+1)
        // No epsilon needed since we're only drawing external faces
        
        std::vector<std::tuple<float, float, float>> cubeCorners = {
            // Back face (z)
            {voxel.x, voxel.y, voxel.z},               // 0: bottom-left-back
            {voxel.x + 1.0f, voxel.y, voxel.z},        // 1: bottom-right-back
            {voxel.x + 1.0f, voxel.y + 1.0f, voxel.z}, // 2: top-right-back
            {voxel.x, voxel.y + 1.0f, voxel.z},        // 3: top-left-back
            // Front face (z + 1)
            {voxel.x, voxel.y, voxel.z + 1.0f},        // 4: bottom-left-front
            {voxel.x + 1.0f, voxel.y, voxel.z + 1.0f}, // 5: bottom-right-front
            {voxel.x + 1.0f, voxel.y + 1.0f, voxel.z + 1.0f}, // 6: top-right-front
            {voxel.x, voxel.y + 1.0f, voxel.z + 1.0f}  // 7: top-left-front
        };
        
        // Project all corners to screen space
        std::vector<ImVec2> screenCorners;
        for (const auto& corner : cubeCorners) {
            screenCorners.push_back(projectToScreen(std::get<0>(corner), std::get<1>(corner), std::get<2>(corner)));
        }
        
        // Color based on value - make fully opaque to avoid blending seams
        ImU32 voxelColor;
        ImU32 borderColor = IM_COL32(0, 0, 0, 255); // Black border
        
        if (voxel_data.dtype() == nb::dtype<float>()) {
            // Color based on value intensity
            uint8_t intensity = (uint8_t)(std::min(std::abs((float)voxel.value) * 255.0f, 255.0f));
            voxelColor = voxel.value > 0 ? 
                IM_COL32(intensity, intensity/2, 0, 255) :  // Orange for positive - fully opaque
                IM_COL32(0, intensity/2, intensity, 255);   // Blue for negative - fully opaque
        } else {
            // Color based on value
            uint8_t r = (uint8_t)((voxel.value * 67) % 256);
            uint8_t g = (uint8_t)((voxel.value * 131) % 256);
            uint8_t b = (uint8_t)((voxel.value * 197) % 256);
            voxelColor = IM_COL32(r, g, b, 255); // Fully opaque
        }
        
        // Define faces with neighbor checks - only draw external faces
        struct Face {
            std::vector<int> indices;
            float centerX, centerY, centerZ;
            float viewDepth; // Depth from camera perspective
            std::string name;
            int neighborX, neighborY, neighborZ; // Neighbor position to check
        };
        
        // Helper function to transform a point and get view depth
        auto getViewDepth = [&](float x, float y, float z) -> float {
            return cameraDepthFn(x, y, z); // camera-space z
        };
        
        // Define all potential faces with their neighbor positions
        std::vector<Face> potentialFaces = {
            // Only include face if neighbor in that direction is empty
            {{0, 1, 2, 3}, voxel.x + 0.5f, voxel.y + 0.5f, voxel.z, 0.0f, "Back", (int)voxel.x, (int)voxel.y, (int)voxel.z - 1},
            {{4, 5, 6, 7}, voxel.x + 0.5f, voxel.y + 0.5f, voxel.z + 1.0f, 0.0f, "Front", (int)voxel.x, (int)voxel.y, (int)voxel.z + 1},
            {{0, 1, 5, 4}, voxel.x + 0.5f, voxel.y, voxel.z + 0.5f, 0.0f, "Bottom", (int)voxel.x, (int)voxel.y - 1, (int)voxel.z},
            {{3, 2, 6, 7}, voxel.x + 0.5f, voxel.y + 1.0f, voxel.z + 0.5f, 0.0f, "Top", (int)voxel.x, (int)voxel.y + 1, (int)voxel.z},
            {{0, 3, 7, 4}, voxel.x, voxel.y + 0.5f, voxel.z + 0.5f, 0.0f, "Left", (int)voxel.x - 1, (int)voxel.y, (int)voxel.z},
            {{1, 2, 6, 5}, voxel.x + 1.0f, voxel.y + 0.5f, voxel.z + 0.5f, 0.0f, "Right", (int)voxel.x + 1, (int)voxel.y, (int)voxel.z}
        };
        
        // Filter faces - only keep external faces (no solid neighbor)
        std::vector<Face> faces;
        for (auto& face : potentialFaces) {
            if (!hasVoxelAt(face.neighborX, face.neighborY, face.neighborZ)) {
                face.viewDepth = getViewDepth(face.centerX, face.centerY, face.centerZ);
                faces.push_back(face);
            }
        }
        
        // Sort faces by view depth (back to front from camera perspective)
        std::sort(faces.begin(), faces.end(), 
                  [](const Face& a, const Face& b) { return a.viewDepth < b.viewDepth; });
        
        // Draw faces in depth-sorted order
        for (size_t faceIndex = 0; faceIndex < faces.size(); faceIndex++) {
            const auto& face = faces[faceIndex];
            const auto& indices = face.indices;
            
            // Draw filled face
            drawList->AddQuadFilled(
                screenCorners[indices[0]], 
                screenCorners[indices[1]], 
                screenCorners[indices[2]], 
                screenCorners[indices[3]], 
                voxelColor
            );
            
            // Draw border if enabled
            if (showVoxelBorders) {
                drawList->AddQuad(
                    screenCorners[indices[0]], 
                    screenCorners[indices[1]], 
                    screenCorners[indices[2]], 
                    screenCorners[indices[3]], 
                    borderColor, 
                    1.0f
                );
            }
            
            // Debug: Show face rendering order and depth values
            if (showDebugFaceOrder) {
                // Calculate face center in screen space for label positioning
                ImVec2 faceCenter = ImVec2(
                    (screenCorners[indices[0]].x + screenCorners[indices[1]].x + screenCorners[indices[2]].x + screenCorners[indices[3]].x) * 0.25f,
                    (screenCorners[indices[0]].y + screenCorners[indices[1]].y + screenCorners[indices[2]].y + screenCorners[indices[3]].y) * 0.25f
                );
                
                // Create debug label with face name, render order, and depth
                char debugLabel[64];
                snprintf(debugLabel, sizeof(debugLabel), "%s\n#%zu\nD:%.2f", 
                        face.name.c_str(), faceIndex, face.viewDepth);
                
                // Draw debug info with background for readability
                ImVec2 textSize = ImGui::CalcTextSize(debugLabel);
                ImVec2 bgMin = ImVec2(faceCenter.x - textSize.x * 0.5f - 2, faceCenter.y - textSize.y * 0.5f - 2);
                ImVec2 bgMax = ImVec2(faceCenter.x + textSize.x * 0.5f + 2, faceCenter.y + textSize.y * 0.5f + 2);
                
                // Semi-transparent background
                drawList->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180));
                drawList->AddRect(bgMin, bgMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);
                
                // Debug text
                ImVec2 textPos = ImVec2(faceCenter.x - textSize.x * 0.5f, faceCenter.y - textSize.y * 0.5f);
                drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), debugLabel);
                
                // Draw arrow pointing from face center to camera direction
                ImVec2 arrowEnd = ImVec2(faceCenter.x, faceCenter.y - 20);
                drawList->AddLine(faceCenter, arrowEnd, IM_COL32(255, 255, 0, 255), 2.0f);
                drawList->AddTriangleFilled(
                    ImVec2(arrowEnd.x, arrowEnd.y - 5),
                    ImVec2(arrowEnd.x - 3, arrowEnd.y + 2),
                    ImVec2(arrowEnd.x + 3, arrowEnd.y + 2),
                    IM_COL32(255, 255, 0, 255)
                );
            }
        }
    }
}

// Display transformation matrix and voxel information
void renderTransformationInfo(nb::ndarray<nb::numpy>& voxel_data, float objectMatrix[16]) {
    ImGui::Separator();

    // Display voxel grid dimensions
    if (voxel_data.ndim() >= 2) {
        ImGui::Text("Voxel Grid: %zux%zu", voxel_data.shape(0), voxel_data.shape(1));
        if (voxel_data.ndim() >= 3) {
            ImGui::SameLine();
            ImGui::Text("x%zu", voxel_data.shape(2));
        }
    } else {
        size_t totalSize = 1;
        for (size_t i = 0; i < voxel_data.ndim(); i++) {
            totalSize *= voxel_data.shape(i);
        }
        ImGui::Text("Voxel Data Length: %zu", totalSize);
    }

    // Display transformation matrix in a more compact way
    if (ImGui::CollapsingHeader("Transform Matrix", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%.2f %.2f %.2f | %.2f", objectMatrix[0], objectMatrix[4], objectMatrix[8], objectMatrix[12]);
        ImGui::Text("%.2f %.2f %.2f | %.2f", objectMatrix[1], objectMatrix[5], objectMatrix[9], objectMatrix[13]);
        ImGui::Text("%.2f %.2f %.2f | %.2f", objectMatrix[2], objectMatrix[6], objectMatrix[10], objectMatrix[14]);
        ImGui::Text("%.2f %.2f %.2f | %.2f", objectMatrix[3], objectMatrix[7], objectMatrix[11], objectMatrix[15]);
    }
}

// Draw coordinate axes (X=Red, Y=Green, Z=Blue)
void drawCoordinateAxes(ImDrawList* drawList, std::function<ImVec2(float, float, float)> projectToScreen) {
    // Origin point
    ImVec2 origin = projectToScreen(0.0f, 0.0f, 0.0f);
    
    // Axis endpoints - extend to a reasonable length for visibility
    float axisLength = 5.0f; // Fixed length for visibility
    ImVec2 xAxisEnd = projectToScreen(axisLength, 0.0f, 0.0f);
    ImVec2 yAxisEnd = projectToScreen(0.0f, axisLength, 0.0f);
    ImVec2 zAxisEnd = projectToScreen(0.0f, 0.0f, axisLength);
    
    // Draw axes with different colors
    // X-axis (Red)
    drawList->AddLine(origin, xAxisEnd, IM_COL32(255, 100, 100, 255), 3.0f);
    // Y-axis (Green)  
    drawList->AddLine(origin, yAxisEnd, IM_COL32(100, 255, 100, 255), 3.0f);
    // Z-axis (Blue)
    drawList->AddLine(origin, zAxisEnd, IM_COL32(100, 100, 255, 255), 3.0f);
    
    // Add axis labels
    ImVec2 textOffset(5, 5);
    drawList->AddText(ImVec2(xAxisEnd.x + textOffset.x, xAxisEnd.y + textOffset.y), IM_COL32(255, 100, 100, 255), "X");
    drawList->AddText(ImVec2(yAxisEnd.x + textOffset.x, yAxisEnd.y + textOffset.y), IM_COL32(100, 255, 100, 255), "Y");
    drawList->AddText(ImVec2(zAxisEnd.x + textOffset.x, zAxisEnd.y + textOffset.y), IM_COL32(100, 100, 255, 255), "Z");
    
    // Draw origin marker
    drawList->AddCircleFilled(origin, 4.0f, IM_COL32(255, 255, 255, 255));
    drawList->AddText(ImVec2(origin.x + textOffset.x, origin.y + textOffset.y), IM_COL32(255, 255, 255, 255), "O");
}

// Draw 3D grid with unit measurements
void drawGrid(ImDrawList* drawList, std::function<ImVec2(float, float, float)> projectToScreen, float zoom) {
    const float gridStep = 1.0f; // Grid spacing of 1 unit (matches voxel size)
    const float gridExtent = 20.0f; // Grid extends from 0 to 20 for good coverage
    const ImU32 gridColor = IM_COL32(80, 80, 80, 128); // Semi-transparent gray
    const ImU32 majorGridColor = IM_COL32(120, 120, 120, 180); // Slightly brighter for major lines
    
    // Draw grid lines parallel to XY plane (at different Z levels)
    for (float z = 0; z <= gridExtent; z += gridStep) {
        bool isMajorZ = (fmod(z, 5.0f) < 0.01f); // Major lines every 5 units
        ImU32 currentColor = isMajorZ ? majorGridColor : gridColor;
        
        // Horizontal lines (parallel to X-axis)
        for (float y = 0; y <= gridExtent; y += gridStep) {
            ImVec2 start = projectToScreen(0, y, z);
            ImVec2 end = projectToScreen(gridExtent, y, z);
            drawList->AddLine(start, end, currentColor, isMajorZ ? 1.5f : 1.0f);
        }
        
        // Vertical lines (parallel to Y-axis)
        for (float x = 0; x <= gridExtent; x += gridStep) {
            ImVec2 start = projectToScreen(x, 0, z);
            ImVec2 end = projectToScreen(x, gridExtent, z);
            drawList->AddLine(start, end, currentColor, isMajorZ ? 1.5f : 1.0f);
        }
    }
    
    // Draw grid lines parallel to XZ plane (at different Y levels)
    for (float y = 0; y <= gridExtent; y += gridStep) {
        bool isMajorY = (fmod(y, 5.0f) < 0.01f); // Major lines every 5 units
        ImU32 currentColor = isMajorY ? majorGridColor : gridColor;
        
        // Lines parallel to Z-axis
        for (float x = 0; x <= gridExtent; x += gridStep) {
            ImVec2 start = projectToScreen(x, y, 0);
            ImVec2 end = projectToScreen(x, y, gridExtent);
            drawList->AddLine(start, end, currentColor, isMajorY ? 1.5f : 1.0f);
        }
    }
    
    // Draw grid lines parallel to YZ plane (at different X levels)
    for (float x = 0; x <= gridExtent; x += gridStep) {
        bool isMajorX = (fmod(x, 5.0f) < 0.01f); // Major lines every 5 units
        ImU32 currentColor = isMajorX ? majorGridColor : gridColor;
        
        // Lines parallel to Z-axis
        for (float y = 0; y <= gridExtent; y += gridStep) {
            ImVec2 start = projectToScreen(x, y, 0);
            ImVec2 end = projectToScreen(x, y, gridExtent);
            drawList->AddLine(start, end, currentColor, isMajorX ? 1.5f : 1.0f);
        }
    }
    
    // Add unit measurements along the axes
    drawUnitMeasurements(drawList, projectToScreen, zoom);
}

// Draw unit measurements and scale indicators
void drawUnitMeasurements(ImDrawList* drawList, std::function<ImVec2(float, float, float)> projectToScreen, float zoom) {
    const ImU32 measurementColor = IM_COL32(200, 200, 200, 255);
    const float tickSize = 3.0f;
    
    // Unit measurements along X-axis (every 5 units for readability)
    for (int i = 0; i <= 20; i += 5) {
        float x = (float)i;
        ImVec2 axisPoint = projectToScreen(x, 0.0f, 0.0f);
        ImVec2 tickStart = ImVec2(axisPoint.x, axisPoint.y - tickSize);
        ImVec2 tickEnd = ImVec2(axisPoint.x, axisPoint.y + tickSize);
        
        drawList->AddLine(tickStart, tickEnd, measurementColor, 2.0f);
        
        // Add measurement label
        char label[8];
        snprintf(label, sizeof(label), "%d", i);
        ImVec2 textPos = ImVec2(axisPoint.x - 8, axisPoint.y + 8);
        drawList->AddText(textPos, measurementColor, label);
    }
    
    // Unit measurements along Y-axis (every 5 units for readability)
    for (int i = 0; i <= 20; i += 5) {
        float y = (float)i;
        ImVec2 axisPoint = projectToScreen(0.0f, y, 0.0f);
        ImVec2 tickStart = ImVec2(axisPoint.x - tickSize, axisPoint.y);
        ImVec2 tickEnd = ImVec2(axisPoint.x + tickSize, axisPoint.y);
        
        drawList->AddLine(tickStart, tickEnd, measurementColor, 2.0f);
        
        // Add measurement label
        char label[8];
        snprintf(label, sizeof(label), "%d", i);
        ImVec2 textPos = ImVec2(axisPoint.x + 8, axisPoint.y - 8);
        drawList->AddText(textPos, measurementColor, label);
    }
    
    // Unit measurements along Z-axis (every 5 units for readability)
    for (int i = 0; i <= 20; i += 5) {
        float z = (float)i;
        ImVec2 axisPoint = projectToScreen(0.0f, 0.0f, z);
        
        // Create a small cross for Z-axis ticks
        drawList->AddLine(ImVec2(axisPoint.x - tickSize, axisPoint.y - tickSize),
                         ImVec2(axisPoint.x + tickSize, axisPoint.y + tickSize), measurementColor, 2.0f);
        drawList->AddLine(ImVec2(axisPoint.x - tickSize, axisPoint.y + tickSize),
                         ImVec2(axisPoint.x + tickSize, axisPoint.y - tickSize), measurementColor, 2.0f);
        
        // Add measurement label
        char label[8];
        snprintf(label, sizeof(label), "%d", i);
        ImVec2 textPos = ImVec2(axisPoint.x + 8, axisPoint.y + 8);
        drawList->AddText(textPos, measurementColor, label);
    }
}


