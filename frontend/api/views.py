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
import celery

from frontend.tasks import blast


bp = Blueprint('api', __name__, url_prefix="/ajax")


@bp.route("/blast/submit", methods=['POST'])
def blast_submit() -> str:

    data = request.get_json()
    if data is None:
        return ""

    task = blast.s(data["query"], data["db"], data, data["reportLimit"])

    result = task.delay()

    return result.id


@bp.route("/blast/status", methods=['POST'])
def blast_status() -> str:

    data = request.get_json()
    if data is None:
        return ""

    task_id = data["id"]

    result = celery.AsyncResult(task_id)

    return result.status


@bp.route("/blast/result/", methods=['GET'])
def blast_result() -> str:

    task_id = request.args.get('job')
    if task_id is None:
        return ""

    result = celery.AsyncResult(task_id)

    return jsonify(result.get())
