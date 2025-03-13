from collections import OrderedDict
from subprocess import run, CalledProcessError
import traceback

from flask import Blueprint, render_template, current_app, request


bp = Blueprint('dashboard', __name__)



@bp.route("/", methods=['GET'])
def index():
    return render_template("index.html")


@bp.route("/search", methods=['GET'])
def search():
    max_results = 5

    q = request.args.get('q')
    db = request.args.get('db')

    databank_names = set([])
    for databank in current_app.config['databanks']:

        if db == "all" or databank.get('id') == db:
            databank_names.add(databank.get('id'))
            continue

        elif "aliases" in databank:
            for alias in databank.get('aliases'):
                if alias.get("id") == db:
                    databank_names.add(databank.get('name'))
                    break

    results = OrderedDict()
    for databank_name in databank_names:

        results[databank_name] = []

        p = run([current_app.config["mrs_executable"], "query", "-d", databank_name, "-q", q, "--count", "1000000000"],
                capture_output=True)

        if p.returncode != 0:
            raise RuntimeError("mrs returned: \n" + p.stderr.decode("utf_8"))

        text = p.stdout.decode("utf_8")
        for line in text.split('\n'):
            if len(line.strip()) > 0:
                s = line.split()
                id_ = s[0]
                title = " ".join(s[1:])

                results[databank_name].append({ "id": id_, "title": title})

    return render_template("browse.html", query=request.args.get('q'), results=results)


@bp.errorhandler(Exception)
def exception_error_handler(error):
    return "<pre>" + traceback.format_exc() + "</pre>"
