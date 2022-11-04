// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "implot.h"
#include "implot_internal.h"



// Application-specific includes
#include "ImUsrpUi.h"
#include "ImUsrpUiRx.h"

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
// this disables the console (somehow the subsystem in project config doesn't)
//#if defined(_MSC_VER)
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
//#endif

#include <thread>
#include <uhd/usrp/multi_usrp.hpp>
#include <atomic>

// forward declaration
void thread_imgui(std::shared_ptr<ImUsrpUi> imusrpui);

int main(int, char**)
{
    // Application-specific instantiations

    // It is important that we perform the construction of the usrp sptr object in the main thread.
    // This is because it was found that RFNoC enabled USRPs like the X310 rely on the thread context;
    // This means that the thread which calls the make() function must be kept alive in order for the object to continue working as intended.
    // The best way to do this is to construct it ie make() in the main thread, and then everything else can be done in side threads.
    bool usrp_connected = false;

    // Old method to flag between threads
    //volatile int to_make_usrp = 0;
    // NOTE: MAKE SURE THIS IS VOLATILE. OR ELSE YOU MUST DISABLE /O2 or /O1 optimizations for it to work!
    // LIKELY THAT /Og IS THE CULPRIT.
    // UNKNOWN WHETHER GCC SUFFERS FROM THE SAME ISSUE.

    // New C++11 recommendation, via https://stackoverflow.com/questions/4557979/when-to-use-volatile-with-multi-threading
    // This is used instead of a volatile int/bool to flag between threads, marking std::memory_order_relaxed during stores/loads
    std::atomic<bool> atom_make_usrp = false;
    // Construct the UI to pass to secondary thread
    std::shared_ptr<ImUsrpUi> imusrpui = std::make_shared<ImUsrpUi>(atom_make_usrp);
    std::thread uithrd(thread_imgui, imusrpui);
    
    // Spin main thread while waiting for the call to instantiate USRP
    while (!usrp_connected)
    {
        //if (to_make_usrp == 1) // old method
        if (atom_make_usrp.load(std::memory_order_relaxed))
        {
            imusrpui->usrp = uhd::usrp::multi_usrp::make(std::string(imusrpui->device_addr_string));
            
            // Populate initial information
            imusrpui->usrp_initialinfo();
            
            // Flag it
            imusrpui->usrp_ready = true;
            

            usrp_connected = true;
            //to_make_usrp = 0; // set back to 0 to flag the UI, old method
            atom_make_usrp.store(false, std::memory_order_relaxed);
            break;
        }
    }
    printf("USRP constructed\n");


    // join ui thread before exiting
    uithrd.join();

    return 0;
}

void thread_imgui(std::shared_ptr<ImUsrpUi> imusrpui)
{
    
    // For debugging?
    //ImUsrpUiRx rxsim(nullptr);

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw 1;
        //return 1;

    //printf("A: class says %d, Addr: %p\n", *(imusrpui->to_make_usrp), imusrpui->to_make_usrp);

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    //printf("B: class says %d, Addr: %p\n", *(imusrpui->to_make_usrp), imusrpui->to_make_usrp);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImUSRP", NULL, NULL);
    if (window == NULL)
        throw 1;
        //return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    //printf("C: class says %d, Addr: %p\n", *(imusrpui->to_make_usrp), imusrpui->to_make_usrp);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //printf("D: class says %d, Addr: %p\n", *(imusrpui->to_make_usrp), imusrpui->to_make_usrp);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    //printf("E: class says %d, Addr: %p\n", *(imusrpui->to_make_usrp), imusrpui->to_make_usrp);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    //printf("F: class says %d, Addr: %p\n", *(imusrpui->to_make_usrp), imusrpui->to_make_usrp);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            ImGui::Begin("Diagnostics");

            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            // Check for ImDrawIdx bits
            ImGui::Text("ImDrawIdx is %zd bits", sizeof(ImDrawIdx) * 8);
            ImGui::Text("VtxOffset = %s", (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) ? "true" : "false");
            ImGui::End();
        }

        //// 3. Show another simple window.
        //if (show_another_window)
        //{
        //    ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        //    ImGui::Text("Hello from another window!");
        //    if (ImGui::Button("Close Me"))
        //        show_another_window = false;
        //    ImGui::End();
        //}

        // ImPlot Demo
        //ImPlot::ShowDemoWindow();

        // ImUSRP Rendering
        imusrpui->render();
        //rxsim.render();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}