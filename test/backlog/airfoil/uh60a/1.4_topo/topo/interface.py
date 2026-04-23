import sys
from msgpi.dakota_interface import process
import msgpi.logger as logger


logger = logger.initLogger(__name__, cout_level='INFO')

process(sys.argv[1], logger=logger)
