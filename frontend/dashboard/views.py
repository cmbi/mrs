import os
from collections import OrderedDict
from subprocess import run, CalledProcessError
import traceback

from Bio import SeqIO

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

    return render_template("browse.html", db=db, q=q, results=results)


@bp.route("/entry", methods=['GET'])
def entry():
    q = request.args.get('q')
    db = request.args.get('db')

    p = run([current_app.config["mrs_executable"], 'entry', '-d', db, '-e', q],
             capture_output=True)

    if p.returncode != 0:
        raise RuntimeError("mrs returned: \n" + p.stderr.decode("utf_8"))

    text = p.stdout.decode("utf_8")

    mrs_directory = None
    for directory in current_app.config["directories"]:
        if directory.get("id") == "mrs":
            mrs_directory = directory.text

    fasta_path = os.path.join(mrs_directory, f"{db}.m6", "fasta")

    fasta_q = f"|{db}|{q}"
    fasta = ""
    if os.path.isfile(fasta_path):
        with open(fasta_path, 'rt') as fasta_file:
            for record in SeqIO.parse(fasta_file, "fasta"):
                if fasta_q in record.id:
                    fasta += f">{record.description}\n{record.seq}"

    links = []

    return render_template("entry.html", db=db, q=q, text=text, fasta=fasta, links=links)


@bp.errorhandler(Exception)
def exception_error_handler(error):
    return "<pre>" + traceback.format_exc() + "</pre>"
