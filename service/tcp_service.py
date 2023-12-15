from flask import Flask
import sys

app = Flask(__name__)

@app.route('/')
def welcome():
  return "Welcome to this page"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(sys.argv[1]))