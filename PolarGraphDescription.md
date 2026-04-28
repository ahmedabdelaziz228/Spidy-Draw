This technical specification describes a specialized **self-designing drawing robot (PolarGraph)** powered by an **ESP32**, designed for high-precision 2D plotting on various surfaces using an integrated tensioned-string mechanism.

### **1. System Architecture & Electronics**
*   **Microcontroller:** **ESP32** development board, responsible for motion control, data processing, and signal generation.
*   **Actuators:** 
    *   **Four Stepper Motors:** 28BYJ-48 (5V) units used for XY positioning.
    *   **Z-Axis Servo:** A micro servo manages pen lifting and lowering.
*   **Motor Drivers:** **ULN2003** driver boards used to drive the stepper motors efficiently.
*   **Power Supply:** Standard **5V DC / 2A** input.
*   **Wiring:** Organized using **JST connectors** for motors, servo, and power distribution.

### **2. Mechanical Design & Hardware**
*   **Chassis:** A dual-plate design consisting of an octagonal wooden **top plate** and a **bottom base plate**, connected by **4mm brass pillars** for structural rigidity.
*   **Pen Mechanism:** A central assembly featuring a **central spring** to maintain constant downward pressure on the drawing surface.
*   **Tensioning System:** Includes **spring-loaded pulleys/tensioners** that ensure smooth movement and consistent string tension.
*   **Physical Specs:** The device weighs approximately **900 grams** with dimensions of roughly **14 x 18 x 14 cm**.

### **3. Software Logic & Motion Control**
The device operates on an **Integrated Logic** model where the motors are housed *within* the device body, rather than at the corners of an external frame. 

*   **Inverse Kinematics (IK):** The core control loop converts target $(X, Y)$ coordinates into specific motor steps based on the actual geometry of the device. 
*   **Motion Control Pipeline:** 
    1.  Receive **Target (X, Y)**.
    2.  Calculate **Inverse Kinematics** to determine required cord lengths.
    3.  Compute individual **Steps** for each motor ($M1, M2, M3, M4$).
    4.  Execute **Synchronized Stepping** to ensure all four motors reach their delta-positions simultaneously, maintaining linear accuracy.
*   **Precision vs. Approximation:** Unlike traditional plotters that may use angle/distance approximations or assume an external frame, this system calculates unique step counts for each motor based on its internal geometry.

### **4. Coordinate System & Axis Control**
*   **XY-Plane:** Managed by the coordinated pulling/releasing of the four strings by the internal stepper motors.
*   **Z-Axis:** A discrete binary state (Up/Down) controlled by the servo to engage or disengage the pen from the surface.

This configuration requires a firmware implementation capable of handling **real-time IK calculations** and **synchronized multi-axis stepper interrupts** on the ESP32.