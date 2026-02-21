class ImageSet:
    def __init__(self, image_paths: dict[str, str], *args, **kwargs) -> None:
        self.images_path_dict: dict[str, str] = image_paths
        self.images: dict[str, str] = {}

        self.load_images()

    def load_images(self) -> None:
        for key, image_path in self.images_path_dict.items():
            self.images[key] = image_path
