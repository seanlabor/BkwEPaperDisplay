import requests
import logging
import json

from datetime import datetime

# Set up logger
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

# Set up console handler
ch = logging.StreamHandler()
ch.setLevel(logging.INFO)
formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)

# API endpoint
url = "http://192.168.178.51:8050/getOutputData"

try:


    # Get current time in Berlin

    logger.info("connecting to Wechselrichter ...")
    response = requests.get(url, timeout=5)
    response.raise_for_status()  # Raise an error for bad responses (4xx, 5xx)

    # Parse JSON response
    data = response.json()

    print(data)

    # Extract values
    if "data" in data:

        current_power = data['data']['p1'] + data['data']['p2']
        power_today = data['data']['e1'] + data['data']['e2']
        power_until_today = data['data']['te1'] + data['data']['te2']

        logger.info(f"Current power: {current_power: .2f} | Power today: {power_today} | Total Power until today: {power_until_today} \n ")
    else:
        logger.warning("No data found, response is empty!")

except requests.exceptions.RequestException as e:
    logger.warning(f"Connection not successful. Error: {e}")
