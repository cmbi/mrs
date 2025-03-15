from xml.etree import ElementTree
import os

from flask import Flask


def create_app():

    app = Flask("MRS", static_folder='frontend/static', template_folder='frontend/templates')
    app.config["mrs_executable"] = "/usr/local/bin/mrs"
    app.config["blastp_executable"] = "/usr/bin/blastp"

    mrs_config = ElementTree.parse("/usr/local/etc/mrs/mrs-config.xml").getroot()

    mrs_directory = None

    app.config["directories"] = []
    for directory in mrs_config.find('directories'):
        app.config["directories"].append(directory)

        if directory.get("id") == "mrs":
            mrs_directory = directory.text

    def is_blastable(db: str) -> bool:
        return os.path.isfile(os.path.join(mrs_directory, db + ".m6", "blast.psq"))

    alias_databank_names = set([])
    all_databank_names = []
    databank_formats = {}
    databank_parsers = {}
    databanks_blastable = {}
    app.config["databanks"] = []
    for databank in mrs_config.find('databanks'):
        if databank.get("enabled") != "true":
            continue

        if "aliases" in databank:
            for alias in databank.get("aliases"):
                alias_name = alias.get("name")
                alias_databank_names.add(alias_name)

        app.config["databanks"].append(databank)
        all_databank_names.append(databank.get("id"))

        if databank.get("format") is not None:
            databank_formats[databank.get("id")] = databank.get("format")

        if databank.get("parser") is not None:
            databank_parsers[databank.get("id")] = databank.get("parser")

    app.config["databank_parsers"] = databank_parsers

    app.jinja_env.globals.update(all_databank_names=all_databank_names)
    app.jinja_env.globals.update(alias_databank_names=alias_databank_names)
    app.jinja_env.globals.update(databank_formats=databank_formats)
    app.jinja_env.globals.update(is_blastable=is_blastable)

    from frontend.dashboard.views import bp
    app.register_blueprint(bp)

    from frontend.api.views import bp
    app.register_blueprint(bp)

    return app
