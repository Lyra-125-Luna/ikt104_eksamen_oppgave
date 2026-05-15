from flask import Flask, request, Response
import requests
import time
import ipaddress
import json
import logging

# Hide Flask spam logs
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

app = Flask(__name__)

# =========================
# API CONFIG
# =========================

NEWS_API = "https://bbc-news-api.vercel.app/news"

OPENWEATHER_API = "https://api.openweathermap.org/data/2.5/weather"

OPENWEATHER_API_KEY = "7c2f370f13a8b83ba4a67cd6f04428d5"

IPGEOLOCATION_API = "https://api.ipgeolocation.io/ipgeo"

IPGEOLOCATION_API_KEY = "d481d36b33e249d4abe04a1c77428884"


# =========================
# GET CLIENT IP
# =========================

def get_client_ip():

    forwarded = request.headers.get("X-Forwarded-For")

    if forwarded:
        return forwarded.split(",")[0].strip()

    return request.remote_addr


# =========================
# CHECK PRIVATE IP
# =========================

def is_private_ip(ip):

    try:
        return ipaddress.ip_address(ip).is_private
    except:
        return False


# =========================
# GET PUBLIC IP
# =========================

def get_public_ip():

    try:

        response = requests.get(
            "https://api.ipify.org?format=json",
            timeout=5
        )

        return response.json().get("ip")

    except:
        return None


# =========================
# GET LOCATION
# =========================

def get_location_data(ip):

    if not ip or ip in ["127.0.0.1", "::1"] or is_private_ip(ip):

        ip = get_public_ip()

    response = requests.get(
        IPGEOLOCATION_API,
        params={
            "apiKey": IPGEOLOCATION_API_KEY,
            "ip": ip
        },
        timeout=10
    )

    return response.json()


# =========================
# HOME
# =========================

@app.route("/")
def home():

    data = {
        "status": "running",
        "routes": [
            "/news",
            "/info"
        ]
    }

    return Response(
        json.dumps(data),
        status=200,
        mimetype="application/json",
        headers={
            "Connection": "close"
        }
    )


# =========================
# NEWS
# =========================

@app.route("/news")
def news():

    try:

        params = request.args.to_dict()

        if "lang" not in params:
            params["lang"] = "english"

        response = requests.get(
            NEWS_API,
            params=params,
            timeout=10
        )

        news_data = response.json()

        titles = []

        #
        # DEBUG PRINT
        #
        print("\nBBC API RESPONSE:")
        print(json.dumps(news_data, indent=2))

        #
        # Try ALL possible locations
        #

        possible_lists = []

        if isinstance(news_data, list):
            possible_lists.append(news_data)

        if isinstance(news_data, dict):

            for key in news_data:

                value = news_data[key]

                if isinstance(value, list):
                    possible_lists.append(value)

        #
        # Extract titles
        #

        for article_list in possible_lists:

            for article in article_list:

                if isinstance(article, dict):

                    title = article.get("title")

                    if title and title not in titles:

                        titles.append(title)

        #
        # Return result
        #

        return Response(
            json.dumps({
                "count": len(titles),
                "titles": titles
            }),
            status=200,
            mimetype="application/json",
            headers={
                "Connection": "close"
            }
        )

    except Exception as e:

        return Response(
            json.dumps({
                "error": str(e)
            }),
            status=500,
            mimetype="application/json",
            headers={
                "Connection": "close"
            }
        )


# =========================
# INFO
# =========================

# =========================
# INFO
# =========================

@app.route("/info")
def info():

    try:

        client_ip = get_client_ip()

        geo_data = get_location_data(client_ip)

        lat = geo_data.get("latitude")
        lon = geo_data.get("longitude")

        city = geo_data.get("city")
        country = geo_data.get("country_name")

        if not lat or not lon:

            return Response(
                json.dumps({
                    "error": "Could not determine location"
                }),
                status=400,
                mimetype="application/json",
                headers={
                    "Connection": "close"
                }
            )

        weather_response = requests.get(
            OPENWEATHER_API,
            params={
                "lat": lat,
                "lon": lon,
                "appid": OPENWEATHER_API_KEY,
                "units": "metric"
            },
            timeout=10
        )

        weather_data = weather_response.json()

        if "weather" not in weather_data:

            return Response(
                json.dumps({
                    "error": "Weather API failed"
                }),
                status=500,
                mimetype="application/json",
                headers={
                    "Connection": "close"
                }
            )

        #
        # TIME / DATE
        #

        current_time = time.localtime()

        weekday = time.strftime("%A", current_time)

        day = time.strftime("%d", current_time)

        month_name = time.strftime("%B", current_time)

        current_clock = time.strftime("%H:%M:%S", current_time)

        #
        # JSON RESPONSE
        #

        data = {

            "unix_epoch": int(time.time()),

            "weekday": weekday,

            "day": day,

            "month": month_name,

            "time": current_clock,

            "ip": client_ip,

            "city": city,

            "country": country,

            "latitude": lat,

            "longitude": lon,

            "weather": weather_data["weather"][0]["main"],

            "temperature_celsius": weather_data["main"]["temp"]
        }

        return Response(
            json.dumps(data),
            status=200,
            mimetype="application/json",
            headers={
                "Connection": "close"
            }
        )

    except Exception as e:

        return Response(
            json.dumps({
                "error": str(e)
            }),
            status=500,
            mimetype="application/json",
            headers={
                "Connection": "close"
            }
        )


# =========================
# START SERVER
# =========================

if __name__ == "__main__":

    print("\nServer running on:")
    print("http://127.0.0.1:5000")
    print("http://0.0.0.0:5000")
    print("http://10.130.51.252:5000\n")

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False,
        threaded=False
    )