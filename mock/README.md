# ZSS Mock server

This is a mock server of ZSS to demo its capabilities 

## Requirements

To run the mock server you will need [python 3.6+](https://www.python.org/)
  and the flask package installed.

```bash
 pip install Flask
```

## Usage

```python
python server.py
```
This should start the server running on [http://127.0.0.1:5000](http://127.0.0.1:5000) by default

## Authentication

By default, to log in use 'mock' for username and 'pass' for password.

## Configuration

To change the port you can set your environment variable 

```bash
FLASK_RUN_PORT=5000
```

