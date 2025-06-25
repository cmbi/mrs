import os
from typing import Dict, Union
from collections import OrderedDict
from subprocess import run, CalledProcessError
import traceback
from tempfile import gettempdir
from uuid import uuid4
from shutil import rmtree
import xml.etree.ElementTree as ET

from Bio import SeqIO

from frontend.application import celery as celery_app


@celery_app.task(queue="mrs_blast")
def blast(query: str, db: str, params: Dict[str, Union[str, float, int, bool]], report_limit: int):

    run_id = uuid4().hex
    run_path = os.path.join(gettempdir(), run_id)
    os.mkdir(run_path)

    try:
        mrs_directory = None
        for directory in celery_app.conf["directories"]:
            if directory.get("id") == "mrs":
                mrs_directory = directory.text

        db_path = os.path.join(mrs_directory, db + ".m6", "blast")

        fasta_path = os.path.join(run_path, "fasta")
        with open(fasta_path, 'wt') as fasta_file:
            fasta_file.write(query)

        output_path = os.path.join(run_path, "output")

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

        xml = ET.parse(output_path)
    finally:
        rmtree(run_path)

    expect = float(params.get("expect"))

    results = []
    iterations = xml.find('BlastOutput_iterations')
    for it in iterations.findall('Iteration'):
        for hits in it.findall('Iteration_hits'):
            for hit in hits.findall('Hit'):

                result = {}
                result["db"] = db
                result["header"] = hit.find('Hit_def').text
                id_split = result["header"].split()[0].split('|')
                result["id"] = id_split[2]
                if len(id_split) > 3:
                    result["chain"] = id_split[3]
                result["nr"] = int(hit.find("Hit_num").text)
                result["hsps"] = []
                result["score"] = 0
                result["bit_score"] = 0
                result["evalue"] = 1e99
                result["query_length"] = len(query)
                result["subject_length"] = int(hit.find("Hit_len").text)
                result["query_start"] = len(query)
                result["query_end"] = 0
                result["length"] = int(hit.find("Hit_len").text)

                hsps = hit.find('Hit_hsps')
                for hsp in hsps.findall('Hsp'):

                    hsp_result = {}

                    hsp_result["score"] = float(hsp.find("Hsp_score").text)
                    hsp_result["bit_score"] = float(hsp.find("Hsp_bit-score").text)
                    hsp_result["evalue"] = float(hsp.find("Hsp_evalue").text)

                    if expect is not None and hsp_result["evalue"] > expect:
                        continue

                    result["score"] = max(result["score"], hsp_result["score"])
                    result["bit_score"] = max(result["bit_score"], hsp_result["bit_score"])
                    result["evalue"] = min(result["evalue"], hsp_result["evalue"])

                    hsp_result["nr"] = int(hsp.find("Hsp_num").text)
                    hsp_result["identity"] = int(hsp.find("Hsp_identity").text)
                    hsp_result["similarity"] = int(hsp.find("Hsp_positive").text)
                    hsp_result["gaps"] = int(hsp.find("Hsp_gaps").text)

                    hsp_result["length"] = int(hsp.find("Hsp_align-len").text)
                    hsp_result["midline"] = hsp.find("Hsp_midline").text
                    hsp_result["query_alignment"] = hsp.find('Hsp_qseq').text
                    hsp_result["query_start"] = int(hsp.find('Hsp_query-from').text)
                    hsp_result["query_end"] = int(hsp.find('Hsp_query-to').text)
                    hsp_result["subject_alignment"] = hsp.find('Hsp_hseq').text
                    hsp_result["subject_start"] = int(hsp.find('Hsp_hit-from').text)
                    hsp_result["subject_end"] = int(hsp.find('Hsp_hit-to').text)

                    result["query_start"] = min(result["query_start"], hsp_result["query_start"])
                    result["query_end"] = max(result["query_end"], hsp_result["query_end"])

                    result["hsps"].append(hsp_result)

                results.append(result)

    return results
