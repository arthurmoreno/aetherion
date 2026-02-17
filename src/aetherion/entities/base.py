from dataclasses import dataclass


@dataclass(frozen=True)
class Classification:
    main_type: int
    sub_type: int

    def __str__(self):
        return f"{self.main_type}:{self.sub_type}"
