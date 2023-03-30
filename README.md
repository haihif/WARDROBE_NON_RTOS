# Smart Wardrobe
## Introduction
  The Smart Wardrobe is a device that helps you decide what to wear based on the weather outside and the conditions inside your wardrobe. It uses a BME280 sensor to detect temperature and humidity levels inside the wardrobe, retrieves the current weather conditions by sending an HTTP request to OpenWeather API, and uses Chat GPT API to provide a response for the question "What should I wear today?". The device communicates with the Blynk platform and displays the data on various widgets.

## Hardware and Software
  The device is built using ESP32 Dev Kit v1 and BME280 sensor. ESP IDF is used to program the device in C language.

## Dependencies
  The following libraries are required to run the project:

  - ESP IDF
  - BME280 Library
  - HTTP Client Library
  - Openweather API
  - Chat GPT API
## How it works
  The device collects temperature and humidity data from the BME280 sensor and sends it to the Blynk platform using the Blynk library. The data is displayed in a chart widget in real-time. The device also retrieves the current weather conditions by sending an HTTP request to OpenWeather API and displays the weather description in the Blynk terminal widget.

  To get the response for the question "What should I wear today?", the device sends a request to the Chat GPT API, which generates a response based on the weather conditions and the user's preferences. The response is then displayed in the Blynk terminal widget.

## Setup
  To set up the Smart Wardrobe device, follow these steps:

  - Clone the repository.
  - Install the ESP IDF and all necessary libraries.
  - Connect the ESP32 Dev Kit v1 and the BME280 sensor.
  - Set up a Blynk account and create a new project.
  - Get an API key from OpenWeather API and Chat GPT API.
  - Update the code with your Blynk authentication token, OpenWeather API key, and Chat GPT API key.
  - Flash the code onto the ESP32 Dev Kit v1.
  - Run the Blynk app on your mobile device, scan the QR code provided by Blynk, and connect to your project.
## Usage
  To use the Smart Wardrobe device, simply open the Blynk app and navigate to your project. The temperature and humidity levels inside your wardrobe will be displayed in real-time on the chart widget. The current weather conditions and the response for the question "What should I wear today?" will be displayed in the Blynk terminal widget.

## Conclusion
  The Smart Wardrobe is a useful device for anyone who wants to simplify their daily routine. By providing real-time temperature and humidity data and generating clothing recommendations based on the weather and user preferences, the device makes it easy to decide what to wear each day.
## Future Improvements
  - The device could be enhanced to include a camera that takes a picture of the clothes inside the wardrobe and uses computer vision to recommend outfits based on the user's preferences.
  - The device could be integrated with smart home systems to adjust the wardrobe's temperature and humidity levels automatically.
  - The device could be improved to allow users to input their preferences, such as their preferred clothing style, color, and fabric.
  - The device could be made more accurate by including more sensors, such as a light sensor, to detect the lighting conditions inside the wardrobe.
## License
  The Smart Wardrobe project is released under the MIT License.
## Contact
  If you have any questions or suggestions about the Smart Wardrobe project,
  please contact us at haihif2002@gmail.com.
<a href="https://github.com/haihif/WARDROBE_NON_RTOS/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=haihif/WARDROBE_NON_RTOS" />
</a>
Enjoy!!!
