# Spotify-ESP32-Controller
🎵 Spotify Music Controller 🎵 an advanced ESP32-based Spotify Controller integrating FreeRTOS for concurrent task management, enabling seamless parallel processing of Spotify API requests. Implemented hardware interrupts to ensure immediate and reliable button press detection, enhancing user interaction responsiveness. Employed mutexes to achieve robust synchronization between dual cores, ensuring thread-safe operations and efficient resource management. This project demonstrates proficiency in real-time operating systems, interrupt handling, and multi-core synchronization within embedded systems.

Find the demo video [here](https://drive.google.com/file/d/1WGF0odx94TxJIQe2uF5W6il286SFcw9s/view?usp=sharing).


## Features:
    1. Real-Time Track Info: Displays song name, artist, and playback progress on an OLED screen.
    2. Dynamic UI: Implements smooth scrolling for long song titles and a responsive progress bar synced with playback.
    3. Physical Controls: Includes buttons for play/pause, next track, and previous track for a seamless music control experience.
    4. Efficient API Handling: Fetches live data from Spotify with optimized timing to ensure low latency and responsiveness.
    5. Compact and Stylish Design: Combines hardware and software to deliver a polished, standalone music controller.
    6. FreeRTOS Integration: Enables parallel processing of Spotify API requests, improving efficiency and responsiveness.
    7. Hardware Interrupts for Button Presses: Ensures no button press goes unregistered, providing a robust user interface.
    8. Mutex Implementation: Facilitates safe and efficient synchronization between ESP32's dual cores for shared resource management.
    9. Comprehensive Error Handling: Addresses potential errors in API responses, enhancing overall system stability.

## Highlights:
    1. Expertly handles Spotify API integration for live data fetching and parsing.
    2. Developed custom algorithms for smooth OLED scrolling and progress visualization.
    3. Designed with a focus on user experience and responsiveness.
    
***This project showcases the power of embedded systems combined with modern APIs to deliver a feature-rich, interactive music controller.***
