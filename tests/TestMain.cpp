#include <exception>
#include <iostream>
#include <string>

void RunSceneSerializerRoundTripTest();
void RunResourceManagerCacheAndReloadTest();
void RunSceneDrawSmokeTest();

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            std::string arg = argv[1];
            if (arg == "--scene-draw") {
                RunSceneDrawSmokeTest();
                std::cout << "Scene draw smoke test passed.\n";
                return 0;
            }
        }

        RunSceneSerializerRoundTripTest();
        RunResourceManagerCacheAndReloadTest();
        std::cout << "All GotMilked tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed with exception: " << ex.what() << '\n';
        return 1;
    }
}

