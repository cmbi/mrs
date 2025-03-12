
from frontend.wsdl import mrs_search

from flask import Blueprint, render_template, current_app, request


bp = Blueprint('dashboard', __name__)


@bp.route("/", methods=['GET'])
def index():
    return render_template("dashboard/index.html", databanks=[databank.get('id') for databank in current_app.config['databanks']])


@bp.route("/search", methods=['GET'])
def search():

    max_results = 30
    offset = int(request.args.get("count", 0)) * max_results

    results = mrs_search.search(request.args.get('db'), request.args.get('q'), offset, max_results)

    return render_template("dashboard/browse.html", query=request.args.get('q'), results=results)


@bp.errorhandler(Exception)
def exception_error_handler(error):
    return str(error)
