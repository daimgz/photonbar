#ifndef WEATHER_H
#define WEATHER_H

#include <cstring>
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <array>
#include <curl/curl.h>
#include <json-c/json.h>
#include "module.h"
#include <fmt/format.h>
#include "../barElement.h"

class WeatherModule : public Module {
  private:
    // Elementos de la UI
    BarElement baseElement;

    // Configuración de ubicación
    const double lat;
    const double lon;

    // Estado del clima
    time_t lastApiCall;
    double temperature;
    double feelsLike;
    int humidity;
    double windSpeed;
    int weatherCode;
    bool isNight;
    bool showDetails;

    // Descripciones del clima (códigos WMO)
    std::array<const char*, 10> weather_descriptions = {
      "Despejado", "Parcialmente nublado", "Nublado", "Lluvia ligera",
      "Lluvia moderada", "Lluvia fuerte", "Nieve", "Niebla",
      "Tormenta", "Variable"
    };

    // Callback para curl
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp) {
      userp->append((char*)contents, size * nmemb);
      return size * nmemb;
    }

  public:
    WeatherModule():
      Module("weather", false, 600),  // No auto-update, cada 600 segundos (10 min)
      lat(-34.4476799),    // Del Viso, Pilar, Buenos Aires
      lon(-58.8052387),    // Del Viso, Pilar, Buenos Aires
      lastApiCall(0),
      temperature(0.0),
      feelsLike(0.0),
      humidity(0),
      windSpeed(0.0),
      weatherCode(0),
      isNight(false),
      showDetails(false)
    {
      // Inicializar curl
      curl_global_init(CURL_GLOBAL_DEFAULT);

      // Configurar elemento base
      baseElement.moduleName = name;

      // Click izquierdo: toggle detalles
      baseElement.setEvent(BarElement::CLICK_LEFT, [this]() {
        showDetails = !showDetails;
        update();
        if (renderFunction) {
          renderFunction();
        }
      });

      // Color base del texto
      baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));

      elements.push_back(&baseElement);
    }

    ~WeatherModule() {
      curl_global_cleanup();
    }

    bool initialize() override {
      // Realizar primera llamada a la API
      return fetchWeatherData();
    }

    void update() override {
      time_t now = time(nullptr);

      // Solo llamar a la API si han pasado 10 minutos
      if ((now - lastApiCall) >= 600) {
        if (fetchWeatherData()) {
          lastApiCall = now;
        }
      }

      generateBuffer();
      lastUpdate = time(nullptr);
    }

  private:
    bool fetchWeatherData() {
      CURL *curl;
      CURLcode res;
      std::string readBuffer;

      curl = curl_easy_init();
      if(!curl) {
        fprintf(stderr, "[WeatherModule] Failed to initialize curl\n");
        return false;
      }

      // Construir URL para Open-Meteo API
      std::string url = fmt::format(
        "https://api.open-meteo.com/v1/forecast?"
        "latitude={:.6f}&longitude={:.6f}&"
        "current_weather=true&"
        "hourly=temperature_2m,apparent_temperature,relativehumidity_2m,windspeed_10m&"
        "timezone=America/Argentina/Buenos_Aires",
        lat, lon
      );

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 segundos timeout

      res = curl_easy_perform(curl);

      bool success = false;
      if(res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
          success = parseWeatherJson(readBuffer);
        } else {
          fprintf(stderr, "[WeatherModule] HTTP error: %ld\n", http_code);
        }
      } else {
        fprintf(stderr, "[WeatherModule] Curl error: %s\n", curl_easy_strerror(res));
      }

      curl_easy_cleanup(curl);
      return success;
    }

    bool parseWeatherJson(const std::string& json_str) {
      json_object *root = json_tokener_parse(json_str.c_str());
      if (!root) {
        fprintf(stderr, "[WeatherModule] Failed to parse JSON\n");
        return false;
      }

      json_object *current_weather;
      if (!json_object_object_get_ex(root, "current_weather", &current_weather)) {
        fprintf(stderr, "[WeatherModule] No current_weather in JSON\n");
        json_object_put(root);
        return false;
      }

      // Extraer temperatura
      json_object *temp_obj;
      if (json_object_object_get_ex(current_weather, "temperature", &temp_obj)) {
        temperature = json_object_get_double(temp_obj);
      }

      // Extraer código del clima
      json_object *weathercode_obj;
      if (json_object_object_get_ex(current_weather, "weathercode", &weathercode_obj)) {
        weatherCode = json_object_get_int(weathercode_obj);
      }

      // Extraer si es de noche
      json_object *is_day_obj;
      if (json_object_object_get_ex(current_weather, "is_day", &is_day_obj)) {
        isNight = !json_object_get_boolean(is_day_obj);
      }

      // Extraer datos horarios para feels like, humedad y viento
      json_object *hourly;
      if (json_object_object_get_ex(root, "hourly", &hourly)) {
        json_object *temp_2m_array;
        json_object *feels_array;
        json_object *humidity_array;
        json_object *wind_array;

        if (json_object_object_get_ex(hourly, "temperature_2m", &temp_2m_array) &&
            json_object_object_get_ex(hourly, "apparent_temperature", &feels_array) &&
            json_object_object_get_ex(hourly, "relativehumidity_2m", &humidity_array) &&
            json_object_object_get_ex(hourly, "windspeed_10m", &wind_array)) {

          // Usar el primer valor (hora actual)
          if (json_object_array_length(temp_2m_array) > 0) {
            feelsLike = json_object_get_double(json_object_array_get_idx(feels_array, 0));
            humidity = json_object_get_int(json_object_array_get_idx(humidity_array, 0));
            windSpeed = json_object_get_double(json_object_array_get_idx(wind_array, 0));
          }
        }
      }

      json_object_put(root);

      fprintf(stderr, "[WeatherModule] Weather updated: %.1f°C, code: %d\n",
              temperature, weatherCode);

      return true;
    }

    const char* getWeatherDescription() {
      // Simplificación de códigos WMO a descripciones
      switch(weatherCode) {
        case 0: return isNight ? u8"\U000f0594" : u8"\U000f0599";  // Despejado
        case 1:
        case 2:
        case 3: return isNight ? u8"\U000f0f31" : u8"\U000f0595";  // Parcialmente nublado
        case 45:
        case 48: return u8"\U000f0591";  // Niebla
        case 51:
        case 53:
        case 55: return u8"\U000f0f33";  // Lluvia ligera
        case 56:
        case 57: return u8"\U000f0597";  // Lluvia helada ligera
        case 61:
        case 63:
        case 65:
        case 66:
        case 67: return u8"\U000f0596";  // 󰖖 Lluvia helada fuerte
        case 71:
        case 73:
        case 75: return u8"\U000f0f36";  // Nieve
        case 77: return u8"\U000f0592";  // Granizo
        case 80:
        case 81:
        case 82: return u8"\U000f067e";  // 󰖓 Tormentas
        case 85:
        case 86: return u8"\U000f0598";  // Nevadas ligeras
        default: return "\uf2c7";  // Desconocido
      }
    }

    void generateBuffer() {
      std::string content;

      // Icono del clima y temperatura principal
      content += fmt::format("{} {:.1f}°C", getWeatherDescription(), temperature);

      if (showDetails) {
        content += fmt::format(
          " | ST: {:.1f}°C | H: {}% | V: {:.1f}km/h",
          feelsLike, humidity, windSpeed
        );
      }

      // Copiar al buffer del elemento
      baseElement.contentLen = snprintf(
        baseElement.content,
        CONTENT_MAX_LEN,
        "%s",
        content.c_str()
      );

      baseElement.dirtyContent = true;

      // Cambiar color según estado
      if (temperature > 30) {
        // Muy caliente - rojo
        baseElement.foregroundColor = Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255));
      } else if (temperature < 5) {
        // Muy frío - azul
        baseElement.foregroundColor = Color::parse_color("#6BB6FF", NULL, Color(107, 182, 255, 255));
      } else {
        // Normal - color por defecto
        baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
      }
    }
};

#endif // WEATHER_H
