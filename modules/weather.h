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

class WeatherModule : public Module {
  public:
    WeatherModule():
      Module("weather"),
      lat(-34.4476799),    // Del Viso, Pilar, Buenos Aires
      lon(-58.8052387),    // Del Viso, Pilar, Buenos Aires
      last_api_call(0),
      temperature(0.0),
      feels_like(0.0),
      humidity(0),
      wind_speed(0.0),
      weather_code(0),
      is_night(false),
      show_details(false)
    {
      // Configurar actualización cada 10 minutos (600 segundos)
      setSecondsPerUpdate(600);
      update_per_iteration = false; // No actualizar en cada ciclo

      // Inicializar curl
      curl_global_init(CURL_GLOBAL_DEFAULT);
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
      if ((now - last_api_call) >= 600) {
        if (fetchWeatherData()) {
          last_api_call = now;
        }
      }

      generateBuffer();
    }

    void event(const char* eventValue) override {
      if (strstr(eventValue, "wt_click")) {
        show_details = !show_details;
        markForUpdate();
      }
    }

  private:
    // Configuración de ubicación
    const double lat;
    const double lon;

    // Estado del clima
    time_t last_api_call;
    double temperature;
    double feels_like;
    int humidity;
    double wind_speed;
    int weather_code;
    bool is_night;
    bool show_details;

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
          success = parseWeatherJSON(readBuffer);
        } else {
          fprintf(stderr, "[WeatherModule] HTTP error: %ld\n", http_code);
        }
      } else {
        fprintf(stderr, "[WeatherModule] Curl error: %s\n", curl_easy_strerror(res));
      }

      curl_easy_cleanup(curl);
      return success;
    }

    bool parseWeatherJSON(const std::string& json_str) {
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
        weather_code = json_object_get_int(weathercode_obj);
      }

      // Extraer si es de noche
      json_object *is_day_obj;
      if (json_object_object_get_ex(current_weather, "is_day", &is_day_obj)) {
        is_night = !json_object_get_boolean(is_day_obj);
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
            feels_like = json_object_get_double(json_object_array_get_idx(feels_array, 0));
            humidity = json_object_get_int(json_object_array_get_idx(humidity_array, 0));
            wind_speed = json_object_get_double(json_object_array_get_idx(wind_array, 0));
          }
        }
      }

      json_object_put(root);

      fprintf(stderr, "[WeatherModule] Weather updated: %.1f°C, code: %d\n",
              temperature, weather_code);

      return true;
    }

    const char* getWeatherDescription() {
      // Simplificación de códigos WMO a descripciones
      switch(weather_code) {
        case 0: return is_night ? "\uf186" : "\uf522";  // Despejado
        case 1:
        case 2:
        case 3: return is_night ? "☁️" : "⛅";  // Parcialmente nublado
        case 45:
        case 48: return "🌫️";  // Niebla
        case 51:
        case 53:
        case 55: return "🌦️";  // Lluvia ligera
        case 56:
        case 57: return "🌨️";  // Lluvia helada ligera
        case 61:
        case 63:
        case 65: return "🌧️";  // Lluvia moderada/fuerte
        case 66:
        case 67: return "🌨️";  // Lluvia helada fuerte
        case 71:
        case 73:
        case 75: return "❄️";  // Nieve
        case 77: return "🌨️";  // Granos de hielo
        case 80:
        case 81:
        case 82: return "⛈️";  // Tormentas
        case 85:
        case 86: return "🌨️";  // Nevadas ligeras
        default: return "🌡️";  // Desconocido
      }
    }

    void generateBuffer() {
      buffer = "%{A1:wt_click:}";

      // Icono del clima y temperatura principal
      buffer += fmt::format("{} {:.1f}°C", getWeatherDescription(), temperature);

      if (show_details) {
        buffer += fmt::format(
          " | ST: {:.1f}°C | H: {}% | V: {:.1f}km/h",
          feels_like, humidity, wind_speed
        );
      }

      buffer += "%{A}";
    }
};

#endif // WEATHER_H
