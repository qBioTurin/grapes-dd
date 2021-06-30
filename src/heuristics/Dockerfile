FROM python:3.8-slim 

RUN pip install --no-cache-dir scipy numpy pandas 
RUN pip install --no-cache-dir openpyxl
COPY entropy.py collect_orderings.py organize_results.py /usr/local/bin/

# ENTRYPOINT [ "entropy.py" ]