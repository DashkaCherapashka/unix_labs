from flask import Flask, request, jsonify
import redis
import uuid
import os

app = Flask(__name__)
redis_client = redis.Redis(host='redis', port=6379, db=0)

@app.route('/task', methods=['POST'])
def create_task():
    data = request.json
    task_id = str(uuid.uuid4())
    task_data = {'id': task_id, 'data': data.get('data', 'default'), 'host': os.uname().nodename}
    redis_client.lpush('task_queue', str(task_data))
    return jsonify({'task_id': task_id, 'status': 'queued', 'processed_by': os.uname().nodename}), 201

@app.route('/health', methods=['GET'])
def health():
    return jsonify({'status': 'healthy', 'host': os.uname().nodename}), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)