import redis
import time
import json
import os

def process_task(task_data):
    print(f"Worker {os.uname().nodename} processing task: {task_data}")
    time.sleep(5)
    print(f"Task {task_data['id']} completed by {os.uname().nodename}")

if __name__ == '__main__':
    redis_client = redis.Redis(host='redis', port=6379, db=0)
    print(f"Worker {os.uname().nodename} started, waiting for tasks...")
    while True:
        _, task_bytes = redis_client.brpop('task_queue')
        task_data = json.loads(task_bytes.decode('utf-8'))
        process_task(task_data)