from typing import List, Dict
import xml.etree.ElementTree as ET


def get_uniprot_fasta(db: str, id_: str, text: str) -> str:

    fasta = ""
    title = ""
    for line in text.split('\n'):

        if line.startswith("DE   RecName: Full="):
            title = line.split('=')[1].strip()

        if line.startswith("SQ  "):
            fasta += f">gnl|{db}|{id_} {title}\n"

        elif line.startswith("    "):
            fasta += line.replace(" ", "") + "\n"

    return fasta


def get_pdbfinder_fasta(db: str, id_: str, text: str) -> str:

    fasta = ""
    chain_id = ""
    title = ""
    for line in text.split('\n'):

        i = line.find(":")
        key = line[:i].strip()
        value = line[i + 1:].strip()

        if key == "Header":
            title = value

        elif key == "Ch-Auth-ID":
            chain_id = value

        elif key == "Sequence":
            sequence=value
            fasta += f">gnl|{db}|{id_}|{chain_id} {title}\n{sequence}\n"

    return fasta


def parse_blast_job(path: str) -> Dict[str, str]:

    xml = ET.parse(path)

    params = {}

    root = xml.getroot()

    d = {}
    for child in root:
        d[child.tag] = child.text

    return d


def parse_blast_results(path: str, db: str, query: str, report_limit: int, expect: float) -> List:

    xml = ET.parse(path)

    results = []
    iterations = xml.find('BlastOutput_iterations')
    for it in iterations.findall('Iteration'):
        for hits in it.findall('Iteration_hits'):
            for hit_index, hit in enumerate(hits.findall('Hit')):

                if hit_index >= report_limit:
                    break

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


fasta_parsers = {
    "uniprot": get_uniprot_fasta,
    "pdbfinder": get_pdbfinder_fasta,
}
