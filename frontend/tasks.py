import os
from typing import Dict, Union
from collections import OrderedDict
from subprocess import run, CalledProcessError
import traceback
from tempfile import gettempdir
from uuid import uuid4
from shutil import rmtree

from Bio import SeqIO

from frontend.application import celery as celery_app
from frontend.parse import parse_blast_results


@celery_app.task(bind=True, queue="mrs_blast")
def blast(self, query: str, db: str, params: Dict[str, Union[str, float, int, bool]]):

    mrs_directory = None
    blast_directory = None
    for directory in celery_app.conf["directories"]:
        if directory.get("id") == "mrs":
            mrs_directory = directory.text

        if directory.get('id') == "blast":
            blast_directory = directory.text

    run_id = self.request.id

    db_path = os.path.join(mrs_directory, db + ".m6", "blast")

    job_path = os.path.join(blast_directory, f"{run_id}.job")
    with open(job_path, 'wt') as job_file:
        job_file.write("<blastjob>\n")
        for key, value in params.items():
            job_file.write(f"  <{key}>{value}</{key}>\n")
        job_file.write("</blastjob>\n")

    fasta_path = os.path.join(blast_directory, f"{run_id}.fasta")
    with open(fasta_path, 'wt') as fasta_file:
        fasta_file.write(query)

    output_path = os.path.join(blast_directory, f"{run_id}.xml")

    cmd = [celery_app.conf["blastp_executable"], '-query', fasta_path, '-db', db_path, "-outfmt", "5", "-out", output_path]

    if "matrix" in params:
        cmd += ["-matrix", params.get("matrix")]

    if "wordSize" in params:
        cmd += ["-word_size", params.get("wordSize")]

    if not params.get("gapped", False):
        cmd += ["-ungapped"]

    if "gapOpen" in params:
        cmd += ["-gapopen", params.get("gapOpen")]

    if "gapExtend" in params:
        cmd += ["-gapextend", params.get("gapExtend")]

    if params.get("lowComplexityFilter", False):
        cmd += ["-seg", "yes"]
    else:
        cmd += ["-seg", "no"]

    run(cmd, check=True)

    expect = float(params.get("expect"))

    report_limit = int(params['reportLimit'])

    results = parse_blast_results(output_path, db, query, report_limit, expect)

    return results
