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

from frontend.parse import fasta_parsers


bp = Blueprint('api', __name__, url_prefix="/api")


@bp.route("/blast", methods=['POST'])
def blast():

    data = request.get_json()

    run_id = uuid4().hex
    run_path = os.path.join(gettempdir(), run_id)
    os.mkdir(run_path)

    try:
        mrs_directory = None
        for directory in current_app.config["directories"]:
            if directory.get("id") == "mrs":
                mrs_directory = directory.text

        db_path = os.path.join(mrs_directory, data["db"] + ".m6", "blast")

        fasta_path = os.path.join(run_path, "fasta")
        with open(fasta_path, 'wt') as fasta_file:
            fasta_file.write(data["query"])

        output_path = os.path.join(run_path, "output")

        run([current_app.config["blastp_executable"], '-query', fasta_path, '-db', db_path, "-outfmt", "5", "-out", output_path],
            check=True)

        xml = ET.parse(output_path)
    finally:
        rmtree(run_path)

    results = []
    iterations = xml.find('BlastOutput_iterations')
    for it in iterations.findall('Iteration'):
        for hits in it.findall('Iteration_hits'):
            for hit in hits.findall('Hit'):

                result = {}
                result["header"] = hit.find('Hit_def').text
                result["alignments"] = []

                hsps = hit.find('Hit_hsps')
                for hsp in hsps.findall('Hsp'):

                    alignment = {}

                    alignment["score"] = int(hsp.find("Hsp_score").text)
                    alignment["bit_score"] = int(hsp.find("Hsp_bit-score").text)
                    alignment["evalue"] = float(hsp.find("Hsp_evalue").text)

                    alignment["identity"] = int(hsp.find("Hsp_identity").text)
                    alignment["similarity"] = int(hsp.find("Hsp_positive").text)
                    alignment["gaps"] = int(hsp.find("Hsp_gaps").text)

                    alignment["length"] = int(hsp.find("Hsp_align-len").text)
                    alignment["midline"] = hsp.find("Hsp_midline").text
                    alignment["query_alignment"] = hsp.find('Hsp_qseq').text
                    alignment["query_start"] = int(hsp.find('Hsp_query-from').text)
                    alignment["query_end"] = int(hsp.find('Hsp_query-to').text)
                    alignment["subject_alignment"] = hsp.find('Hsp_hseq').text
                    alignment["subject_start"] = int(hsp.find('Hsp_hit-from').text)
                    alignment["subject_end"] = int(hsp.find('Hsp_hit-to').text)

                    result["alignments"].append(alignment)

                results.append(result)

    return jsonify(results)
