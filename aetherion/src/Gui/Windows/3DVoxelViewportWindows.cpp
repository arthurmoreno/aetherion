#include "Gui/Gui.hpp"

// Forward declarations for helper methods
void renderVoxelDataHeader(nb::ndarray<nb::numpy>& voxel_data);
void renderTransformControls(float translation[3], float rotation[3], float scale[3], 
                           float& viewDistance, float& zoom, bool& matrixChanged);
void renderImGuizmoControls(ImGuizmo::OPERATION& currentGizmoOperation, ImGuizmo::MODE& currentGizmoMode, 
                          bool& useSnap, float snap[3]);
void updateTransformationMatrix(float objectMatrix[16], const float translation[3], 
                              const float rotation[3], const float scale[3], bool matrixChanged);
void setupProjectionMatrix(float cameraProjection[16], float aspect);
void render3DViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data, 
                     float cameraView[16], float cameraProjection[16], float objectMatrix[16],
                     ImGuizmo::OPERATION currentGizmoOperation, ImGuizmo::MODE currentGizmoMode,
                     bool useSnap, float snap[3], float zoom, float viewDistance);

// Main function to render the 3D voxel viewport
void render3DVoxelViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data) {
    // Create 3D Viewport window
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("3D Voxel Viewport")) {
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
    static float translation[3] = { 0.0f, 0.0f, 0.0f };
    static float rotation[3] = { 0.0f, 0.0f, 0.0f };
    static float scale[3] = { 1.0f, 1.0f, 1.0f };
    static float viewDistance = 5.0f;
    static float zoom = 1.0f;
    static bool matrixChanged = false;
    
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;
    static bool useSnap = false;
    static float snap[3] = { 1.0f, 1.0f, 1.0f };
    
    // Get the available content region for layout calculations
    ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    float headerHeight = 80.0f; // Fixed height for header
    float leftPanelWidth = 300.0f; // Fixed width for control panel
    
    // 1. Header Section - Basic Info
    ImGui::BeginChild("HeaderRegion", ImVec2(0, headerHeight), true);
    renderVoxelDataHeader(voxel_data);
    ImGui::EndChild();
    
    // 2. Main Content Section - Split layout
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
    
    // Left panel - Controls
    ImGui::BeginChild("ControlsPanel", ImVec2(leftPanelWidth, 0), true);
    
    // Transform controls
    renderTransformControls(translation, rotation, scale, viewDistance, zoom, matrixChanged);
    
    ImGui::Separator();
    
    // ImGuizmo controls
    renderImGuizmoControls(currentGizmoOperation, currentGizmoMode, useSnap, snap);
    
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right panel - 3D Viewport
    ImGui::BeginChild("ViewportPanel", ImVec2(0, 0), true);
    
    // Update matrices if needed
    updateTransformationMatrix(objectMatrix, translation, rotation, scale, matrixChanged);
    cameraView[14] = -viewDistance; // Update camera view based on view distance
    
    float aspect = ImGui::GetContentRegionAvail().x / ImGui::GetContentRegionAvail().y;
    setupProjectionMatrix(cameraProjection, aspect);
    
    // Render the 3D viewport
    render3DViewport(voxel_data, shared_data, cameraView, cameraProjection, objectMatrix,
                    currentGizmoOperation, currentGizmoMode, useSnap, snap, zoom, viewDistance);
    
    ImGui::EndChild();
    
    ImGui::EndChild(); // MainContent
    
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
                           float& viewDistance, float& zoom, bool& matrixChanged) {
    ImGui::Text("Transform Controls");
    ImGui::Separator();
    
    if (ImGui::SliderFloat3("Translation", translation, -5.0f, 5.0f)) {
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
    if (ImGui::SliderFloat("Zoom", &zoom, 0.1f, 5.0f)) {
        matrixChanged = true;
    }
    
    // Reset and randomize buttons
    if (ImGui::Button("Reset Transform")) {
        translation[0] = translation[1] = translation[2] = 0.0f;
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        scale[0] = scale[1] = scale[2] = 1.0f;
        viewDistance = 5.0f;
        zoom = 1.0f;
        matrixChanged = true;
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
                          bool& useSnap, float snap[3]) {
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
}

// Update the transformation matrix based on current control values
void updateTransformationMatrix(float objectMatrix[16], const float translation[3], 
                              const float rotation[3], const float scale[3], bool matrixChanged) {
    if (!matrixChanged) return;
    
    // Convert degrees to radians
    float rx = rotation[0] * 3.14159f / 180.0f;
    float ry = rotation[1] * 3.14159f / 180.0f;
    float rz = rotation[2] * 3.14159f / 180.0f;
    
    // Create rotation matrices
    float cos_x = cosf(rx), sin_x = sinf(rx);
    float cos_y = cosf(ry), sin_y = sinf(ry);
    float cos_z = cosf(rz), sin_z = sinf(rz);
    
    // Combined rotation matrix (ZYX order)
    float rotMatrix[16] = {
        cos_y * cos_z, -cos_y * sin_z, sin_y, 0,
        sin_x * sin_y * cos_z + cos_x * sin_z, -sin_x * sin_y * sin_z + cos_x * cos_z, -sin_x * cos_y, 0,
        -cos_x * sin_y * cos_z + sin_x * sin_z, cos_x * sin_y * sin_z + sin_x * cos_z, cos_x * cos_y, 0,
        0, 0, 0, 1
    };
    
    // Apply scale and translation to create final transformation matrix
    objectMatrix[0] = rotMatrix[0] * scale[0];
    objectMatrix[1] = rotMatrix[1] * scale[0];
    objectMatrix[2] = rotMatrix[2] * scale[0];
    objectMatrix[3] = rotMatrix[3];
    
    objectMatrix[4] = rotMatrix[4] * scale[1];
    objectMatrix[5] = rotMatrix[5] * scale[1];
    objectMatrix[6] = rotMatrix[6] * scale[1];
    objectMatrix[7] = rotMatrix[7];
    
    objectMatrix[8] = rotMatrix[8] * scale[2];
    objectMatrix[9] = rotMatrix[9] * scale[2];
    objectMatrix[10] = rotMatrix[10] * scale[2];
    objectMatrix[11] = rotMatrix[11];
    
    objectMatrix[12] = translation[0];
    objectMatrix[13] = translation[1];
    objectMatrix[14] = translation[2];
    objectMatrix[15] = 1.0f;
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
                    float zoom, size_t totalSize);
void processFloatVoxelData(const float* data, nb::ndarray<nb::numpy>& voxel_data, 
                          std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize);
void processIntVoxelData(const int* data, nb::ndarray<nb::numpy>& voxel_data,
                        std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize);
void drawVoxelPoints(const std::vector<VoxelPoint>& voxelPoints, ImDrawList* drawList,
                    std::function<ImVec2(float, float, float)> projectToScreen,
                    float zoom, nb::ndarray<nb::numpy>& voxel_data);
void renderTransformationInfo(nb::ndarray<nb::numpy>& voxel_data, float objectMatrix[16]);

// Render the main 3D viewport with voxels and ImGuizmo
void render3DViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data, 
                     float cameraView[16], float cameraProjection[16], float objectMatrix[16],
                     ImGuizmo::OPERATION currentGizmoOperation, ImGuizmo::MODE currentGizmoMode,
                     bool useSnap, float snap[3], float zoom, float viewDistance) {
    
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    
    // Set ImGuizmo viewport
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(contentPos.x, contentPos.y, viewportSize.x, viewportSize.y);
    
    // Helper function to apply 4x4 matrix transformation to a 3D point
    auto transformPoint = [&](float x, float y, float z) -> std::tuple<float, float, float> {
        float tx = objectMatrix[0] * x + objectMatrix[4] * y + objectMatrix[8] * z + objectMatrix[12];
        float ty = objectMatrix[1] * x + objectMatrix[5] * y + objectMatrix[9] * z + objectMatrix[13];
        float tz = objectMatrix[2] * x + objectMatrix[6] * y + objectMatrix[10] * z + objectMatrix[14];
        float tw = objectMatrix[3] * x + objectMatrix[7] * y + objectMatrix[11] * z + objectMatrix[15];
        
        // Perspective divide
        if (tw != 0.0f) {
            tx /= tw;
            ty /= tw;
            tz /= tw;
        }
        
        return std::make_tuple(tx, ty, tz);
    };
    
    // Helper function to project 3D point to 2D screen coordinates
    auto projectToScreen = [&](float x, float y, float z) -> ImVec2 {
        // Apply transformation matrix
        auto [tx, ty, tz] = transformPoint(x, y, z);
        
        // Simple orthographic projection with depth offset and zoom
        float baseScale = 100.0f * zoom;
        float scale = baseScale / (viewDistance + tz * 0.5f); // Scale based on distance
        float screenX = contentPos.x + viewportSize.x * 0.5f + tx * scale;
        float screenY = contentPos.y + viewportSize.y * 0.5f - ty * scale; // Flip Y for screen coordinates
        
        return ImVec2(screenX, screenY);
    };
    
    // Render the 3D voxel visualization
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Draw the voxel container outline using the same 3D coordinate system as the voxels
    // Define the 8 corners of a unit cube in 3D space
    std::vector<std::tuple<float, float, float>> cubeCorners = {
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},  // Back face
        {-1.0f, -1.0f,  1.0f}, {1.0f, -1.0f,  1.0f}, {1.0f, 1.0f,  1.0f}, {-1.0f, 1.0f,  1.0f}   // Front face
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
    
    // Calculate total size for voxel data processing
    size_t totalSize = 1;
    for (size_t i = 0; i < voxel_data.ndim(); i++) {
        totalSize *= voxel_data.shape(i);
    }
    
    // Render voxels based on actual data
    if (totalSize > 0 && voxel_data.data() != nullptr) {
        renderVoxelData(voxel_data, drawList, projectToScreen, zoom, totalSize);
    }
    
    // Use ImGuizmo to manipulate the object
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
    
    // Display transformation info and voxel data summary at the bottom
    renderTransformationInfo(voxel_data, objectMatrix);
}

// Render voxel data points
void renderVoxelData(nb::ndarray<nb::numpy>& voxel_data, ImDrawList* drawList,
                    std::function<ImVec2(float, float, float)> projectToScreen,
                    float zoom, size_t totalSize) {
    
    // Determine grid size based on data dimensions or use reasonable default
    int gridSize = 16;
    if (voxel_data.ndim() >= 2) {
        gridSize = std::min((int)voxel_data.shape(0), 32); // Limit to reasonable size
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
    
    // Sort voxels by depth (back to front for proper alpha blending)
    std::sort(voxelPoints.begin(), voxelPoints.end(), 
              [](const VoxelPoint& a, const VoxelPoint& b) { return a.depth < b.depth; });
    
    // Draw sorted voxels
    drawVoxelPoints(voxelPoints, drawList, projectToScreen, zoom, voxel_data);
}

// Process float voxel data
void processFloatVoxelData(const float* data, nb::ndarray<nb::numpy>& voxel_data, 
                          std::vector<VoxelPoint>& voxelPoints, int gridSize, size_t totalSize) {
    
    // For 3D data visualization
    if (voxel_data.ndim() >= 3) {
        size_t width = voxel_data.shape(0);
        size_t height = voxel_data.shape(1);
        size_t depth = voxel_data.shape(2);
        
        for (size_t x = 0; x < std::min(width, (size_t)gridSize); x++) {
            for (size_t y = 0; y < std::min(height, (size_t)gridSize); y++) {
                for (size_t z = 0; z < std::min(depth, (size_t)gridSize); z++) {
                    size_t index = z * width * height + y * width + x;
                    if (index < totalSize) {
                        float value = data[index];
                        
                        // Only draw non-zero voxels
                        if (std::abs(value) > 0.001f) {
                            // Normalize coordinates to [-1, 1] range
                            float nx = (x / (float)gridSize - 0.5f) * 2.0f;
                            float ny = (y / (float)gridSize - 0.5f) * 2.0f;
                            float nz = (z / (float)gridSize - 0.5f) * 2.0f;
                            
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
        
        for (size_t x = 0; x < std::min(width, (size_t)gridSize); x++) {
            for (size_t y = 0; y < std::min(height, (size_t)gridSize); y++) {
                size_t index = y * width + x;
                if (index < totalSize) {
                    float value = data[index];
                    
                    // Only draw non-zero voxels
                    if (std::abs(value) > 0.001f) {
                        // Normalize coordinates to [-1, 1] range, z=0 for 2D
                        float nx = (x / (float)gridSize - 0.5f) * 2.0f;
                        float ny = (y / (float)gridSize - 0.5f) * 2.0f;
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
        
        for (size_t x = 0; x < std::min(width, (size_t)gridSize); x++) {
            for (size_t y = 0; y < std::min(height, (size_t)gridSize); y++) {
                for (size_t z = 0; z < std::min(depth, (size_t)gridSize); z++) {
                    size_t index = z * width * height + y * width + x;
                    if (index < totalSize) {
                        int value = data[index];
                        
                        if (value != 0) {
                            // Normalize coordinates to [-1, 1] range
                            float nx = (x / (float)gridSize - 0.5f) * 2.0f;
                            float ny = (y / (float)gridSize - 0.5f) * 2.0f;
                            float nz = (z / (float)gridSize - 0.5f) * 2.0f;
                            
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
        
        for (size_t x = 0; x < std::min(width, (size_t)gridSize); x++) {
            for (size_t y = 0; y < std::min(height, (size_t)gridSize); y++) {
                size_t index = y * width + x;
                if (index < totalSize) {
                    int value = data[index];
                    
                    if (value != 0) {
                        // Normalize coordinates to [-1, 1] range, z=0 for 2D
                        float nx = (x / (float)gridSize - 0.5f) * 2.0f;
                        float ny = (y / (float)gridSize - 0.5f) * 2.0f;
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
                    float zoom, nb::ndarray<nb::numpy>& voxel_data) {
    
    for (const auto& voxel : voxelPoints) {
        ImVec2 screenPos = projectToScreen(voxel.x, voxel.y, voxel.z);
        
        // Calculate voxel size based on distance and zoom
        float baseVoxelSize = 8.0f * zoom;
        float voxelSize = baseVoxelSize / (1.0f + std::abs(voxel.depth) * 0.2f);
        voxelSize = std::max(voxelSize, 1.0f); // Minimum size
        
        // Color based on value
        ImU32 voxelColor;
        if (voxel_data.dtype() == nb::dtype<float>()) {
            // Color based on value intensity
            uint8_t intensity = (uint8_t)(std::min(std::abs((float)voxel.value) * 255.0f, 255.0f));
            voxelColor = voxel.value > 0 ? 
                IM_COL32(intensity, intensity/2, 0, 200) :  // Orange for positive
                IM_COL32(0, intensity/2, intensity, 200);   // Blue for negative
        } else {
            // Color based on value
            uint8_t r = (uint8_t)((voxel.value * 67) % 256);
            uint8_t g = (uint8_t)((voxel.value * 131) % 256);
            uint8_t b = (uint8_t)((voxel.value * 197) % 256);
            voxelColor = IM_COL32(r, g, b, 200);
        }
        
        drawList->AddRectFilled(
            ImVec2(screenPos.x - voxelSize/2, screenPos.y - voxelSize/2), 
            ImVec2(screenPos.x + voxelSize/2, screenPos.y + voxelSize/2),
            voxelColor
        );
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


