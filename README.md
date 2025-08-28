# FreeRTOS Simulation on Windows

This repository contains code and setup instructions for running a **FreeRTOS simulation** on Windows using Visual Studio.

---

## Prerequisites

- [Visual Studio](https://visualstudio.microsoft.com/) (latest version recommended)  
- C++ Development Kit (installed via the Visual Studio Installer)  
- Stable internet connection  

> **Note:** Ensure you have only one version of Visual Studio installed. If multiple installations exist, uninstall the older ones before proceeding.

---

## Setup Instructions

1. **Install Visual Studio**  
   - Download and install Visual Studio.  
   - During installation, ensure the **C++ Development Kit** is selected.  

2. **Verify Installation**  
   - Open the *Visual Studio Installer*.  
   - Confirm that only one version of Visual Studio is installed.  

3. **Clone the FreeRTOS Repository**  
   - Open the Visual Studio and then clone using the below repo link
   git clone https://github.com/FreeRTOS/FreeRTOS
   
5. **Open the Project in Visual Studio**
   - Launch Visual Studio.
   - Open the cloned repository folder.
   - Navigate to:
     ```FreeRTOS\Demo\WIN32-MSVC\main.c```
   - Open main.c and make any changes required for your simulation.
   - Build and run the project inside Visual Studio.
