from abc import (
  ABC,
  abstractmethod,
)

class NetworkExposure_API(ABC):
    def __init__(self, name):
        self.name = name
        self._level = 1

    @abstractmethod
    def NetworkResourceAdaptation(self):
        pass