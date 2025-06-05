#include <iostream>
#include <string>
#include <vector>
#include <thread>         // Required for std::thread
#include <atomic>         // Required for std::atomic
#include <chrono>         // Required for std::chrono::milliseconds

#include <opencv2/opencv.hpp> // OpenCV main header
#include <opencv2/videoio.hpp> // For cv::VideoCapture
#include <opencv2/imgproc.hpp> // For cv::cvtColor, cv::Rect, cv::mean
#include <opencv2/highgui.hpp> // For cv::destroyAllWindows (though not strictly needed if no imshow)

// Global atomic flag to signal the processing thread to stop
std::atomic<bool> stop_processing(false);

// Function to open the camera
bool openCamera(cv::VideoCapture& cap, int device) {
    if (!cap.open(device)) {
        std::cerr << "Erro ao abrir a camera no dispositivo " << device << std::endl;
        return false;
    }
    std::cout << "Câmera aberta no dispositivo " << device << std::endl;
    // Optionally set camera properties here, e.g., resolution
    // cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    return true;
}

// Function to close the camera
void closeCamera(cv::VideoCapture& cap) {
    if (cap.isOpened()) {
        cap.release();
        std::cout << "Câmera liberada." << std::endl;
    }
    // cv::destroyAllWindows(); // Only needed if cv::imshow was used.
}

// Function to detect color based on RGB values
std::string detectColorRGB(const cv::Vec3b& pixel) {
    int b = pixel[0];
    int g = pixel[1];
    int r = pixel[2];

    // Color detection logic (adjust thresholds as needed)
    if (r > 200 && g < 80 && b < 80) return "Vermelho";
    if (g > 200 && r < 80 && b < 80) return "Verde";
    if (b > 200 && r < 80 && g < 80) return "Azul";
    if (r > 200 && g > 200 && b < 80) return "Amarelo"; // Yellow
    if (r > 200 && g > 100 && b > 100 && g < 200 && b < 200) return "Rosa"; // Pink (refined to avoid clash with white)
    if (r > 200 && g > 200 && b > 200) return "Branco"; // White (checked before more specific bright colors if order matters)
    if (r < 50 && g < 50 && b < 50) return "Preto";   // Black
    
    return "Indefinido";
}

// Function to be run in a separate thread for processing camera frames
void camera_processing_thread_func(cv::VideoCapture& cap) {
    static std::string ultimaCorDetectada; // Remembers the last color to avoid repetitive prints
    cv::Mat frame;
    int r_val, g_val, b_val;

    std::cout << "Thread de processamento de câmera iniciada." << std::endl;

    while (!stop_processing) {
        if (!cap.isOpened()) {
            if (!stop_processing) { // Avoid error message if we are already stopping
                std::cerr << "Erro: Câmera não está aberta no thread de processamento." << std::endl;
            }
            stop_processing = true; // Signal to stop if camera is unexpectedly closed
            break;
        }

        if (!cap.read(frame)) { // Read a new frame from the camera
            if (!stop_processing) {
                std::cerr << "Erro: Não foi possível ler o frame da câmera." << std::endl;
            }
            // Brief pause before trying again or exiting if stop_processing is set
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (frame.empty()) {
            if (!stop_processing) {
                std::cerr << "Erro: Frame vazio capturado." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Define a Region of Interest (ROI) in the center of the frame
        int rectSize = 20; // Size of the square ROI
        if (frame.cols < rectSize || frame.rows < rectSize) {
            if(!stop_processing) {
                std::cerr << "Erro: Frame muito pequeno para o ROI definido." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        int x = frame.cols / 2;
        int y = frame.rows / 2;
        cv::Rect roi(x - rectSize / 2, y - rectSize / 2, rectSize, rectSize);

        // Extract the ROI
        cv::Mat region = frame(roi);

        // Calculate the mean color of the ROI
        cv::Scalar meanColorScalar = cv::mean(region); // OpenCV's Scalar is BGR

        // Extract B, G, R values
        b_val = static_cast<int>(meanColorScalar[0]);
        g_val = static_cast<int>(meanColorScalar[1]);
        r_val = static_cast<int>(meanColorScalar[2]);

        // Detect the color based on the mean RGB values
        std::string corAtual = detectColorRGB(cv::Vec3b(b_val, g_val, r_val));

        // Print the detected color if it's new and not "Indefinido"
        if (corAtual != "Indefinido" && corAtual != ultimaCorDetectada) {
            std::cout << "Cor detectada: " << corAtual 
                      << " (R=" << r_val << ", G=" << g_val << ", B=" << b_val << ")" << std::endl;
            ultimaCorDetectada = corAtual;
        }
        
        // Optional: uncomment to display the frame (requires a display environment)
        // cv::rectangle(frame, roi, cv::Scalar(0,255,0), 1); // Draw ROI for visualization
        // cv::imshow("Camera Feed", frame);
        // char key = (char)cv::waitKey(1); // Needs highgui window
        // if (key == 27 || key == 'q' || key == 'Q') { // ESC or Q to quit
        //     stop_processing = true;
        // }


        // Add a small delay to control frame processing rate and reduce CPU load
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); // Approx 33 FPS
    }

    std::cout << "Thread de processamento de câmera terminando." << std::endl;
}

int main() {
    cv::VideoCapture camera_capture;
    int camera_device_id = 0; // Common for built-in or first USB camera (try 0, 1, etc.)
                              // For Raspberry Pi camera module, you might need specific gstreamer pipelines
                              // or ensure it's mapped to /dev/video0

    // Attempt to open the camera
    if (!openCamera(camera_capture, camera_device_id)) {
        std::cerr << "Falha ao inicializar a câmera. Saindo." << std::endl;
        return -1;
    }

    std::cout << "Thread principal: Iniciando thread de processamento de câmera." << std::endl;
    std::cout << "Pressione ENTER no console para parar o processamento e sair." << std::endl;

    // Reset the stop flag before starting the thread
    stop_processing = false;

    // Create and start the camera processing thread
    // std::ref() is used to pass the VideoCapture object by reference
    std::thread processing_thread(camera_processing_thread_func, std::ref(camera_capture));

    // Wait for user input (e.g., pressing Enter) in the main thread to stop
    std::cin.get();

    std::cout << "Thread principal: Sinalizando para o thread de processamento parar..." << std::endl;
    // Set the flag to signal the processing thread to stop
    stop_processing = true;

    // Wait for the processing thread to finish its current work and exit gracefully
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
    std::cout << "Thread principal: Thread de processamento finalizado." << std::endl;

    // Release the camera and clean up
    closeCamera(camera_capture);

    std::cout << "Programa terminado." << std::endl;
    return 0;
}