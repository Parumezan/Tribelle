from fastapi import FastAPI
from ultralytics import YOLO
import requests
from threading import Thread

esp_url = "ESP-API-URL"
model = YOLO("trash.pt")
api = FastAPI()
thread = None


def check_capture():
    listOfDetections = []

    for count in range(5):
        resp = requests.get(f"http://{esp_url}/capture")
        with open(f"images/{count}.jpg", "wb") as f:
            f.write(resp.content)

    for count in range(5):
        results = model(f"images/{count}.jpg")
        names = model.names
        for r in results:
            for c in r.boxes.cls:
                listOfDetections.append(names[int(c)])

    mostCommon = max(set(listOfDetections), key=listOfDetections.count)

    if mostCommon == "PLASTIC":
        requests.get(f"http://{esp_url}/plastic")
    elif mostCommon == "PAPER":
        requests.get(f"http://{esp_url}/paper")
    elif mostCommon == "METAL":
        requests.get(f"http://{esp_url}/metal")
    elif mostCommon == "GLASS":
        requests.get(f"http://{esp_url}/glass")
    elif mostCommon == "CARDBOARD":
        requests.get(f"http://{esp_url}/cardboard")
    elif mostCommon == "BIODEGRADABLE":
        requests.get(f"http://{esp_url}/biodegradable")


@api.get("/check-capture")
async def api_check_capture():
    Thread(target=check_capture).start()
    return {"status": "ok"}


@api.get("/check-stream")
async def api_check_stream():
    return {"status": "ok"}
