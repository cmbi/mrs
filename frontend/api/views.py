import os
from collections import OrderedDict
from subprocess import run, CalledProcessError
import traceback
from tempfile import gettempdir
from uuid import uuid4
from shutil import rmtree
import xml.etree.ElementTree as ET

from Bio import SeqIO

from flask import Blueprint, render_template, current_app, request, jsonify


bp = Blueprint('api', __name__, url_prefix="/api")


@bp.route("/blast/submit", methods=['POST'])
def blast_submit() -> str:

    data = request.get_json()

    from frontend.tasks import blast

    task = blast.s(data["query"], data["db"], data, data["reportLimit"])

    result = task.delay()

    response = {"clientId": result.id, "status": result.status.lower()}

    return jsonify(response)


@bp.route("/blast/status", methods=['POST'])
def blast_status() -> str:

    data = request.get_json()

    task_ids = data["jobs"].split(';')

    from frontend.application import celery as celery_app

    statuses = []
    for task_id in task_ids:

        result = celery_app.AsyncResult(task_id)

        status = {
            "id": task_id,
            "status": result.status.lower(),
        }

        if result.status == "SUCCESS":
            rs = result.get()

            status['hitCount'] = len(rs)
            status['bestEValue'] = min([r['evalue'] for r in rs])

        elif result.status == "FAILURE":
            try:
                result.get()
            except Exception as e:
                status['error'] = str(e)

        statuses.append(status)

    return jsonify(statuses)

@bp.route("/blast/result/", methods=['GET'])
def blast_result() -> str:

    task_id = request.args['job']
    hit_number = request.args.get('hit')

    from frontend.application import celery as celery_app
    result = celery_app.AsyncResult(task_id)

    return jsonify(result.get())
