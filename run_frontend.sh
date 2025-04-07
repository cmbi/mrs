sudo -u mrs celery -A frontend.application:celery worker -c 20 &

sudo -u mrs gunicorn -k gevent -b 0.0.0.0:18091 frontend.application:app
