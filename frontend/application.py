from frontend.factory import create_app, create_celery_app

app = create_app()
celery = create_celery_app(app)
