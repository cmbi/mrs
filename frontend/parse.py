

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


fasta_parsers = {
    "uniprot": get_uniprot_fasta,
    "pdbfinder": get_pdbfinder_fasta,
}
