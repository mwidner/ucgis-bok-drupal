<?php

/**
 * A Drush command to export data from the UCGIS BoK Drupal 7 site
 * into a JSON format that can be imported to the Living Textbook.
 *
 * Usage: drush bok-export --limit=100 -s <FILENAME>
 *
 * Generates a JSON file at the given FILENAME path
 * Use --limit option to restrict the max number of records. Default: 100
 *
 * Use -s to strip HTML tags
 */

// ~/PhpstormProjects/bok/public_html$ ../drush bok-export --limit=100 ./export-21-12-20.json

/**
 * Node: represents the data for a node in the LT
 */
class Node {
  /** @var int $id */
  public $id;
  /** @var int $drupalId */
  public $drupalId;
  /** @var string $code */
  public $code;
  /** @var string $title */
  public $title;
  /** @var string $name */
  public $name;
  /** @var string $definition */
  public $definition;
  /** @var string $definitions */
  public $definitions;
  /** @var string $content */
  public $content;

  public static function serialize(Node $node, bool $isStripTags = false): array
  {
    $data = [];

    if ($node->code) {
      $node->title = "[$node->code] $node->title";
    }
    $data['label'] = cleanString($node->title);
    $data['code'] = cleanString($node->code);
    $data['nodeName'] = cleanString($node->name);
    $data['definition'] = cleanString($node->definition);
    $data['content'] = $isStripTags ? cleanString($node->content) : $node->content;
    return $data;
  }
}

/**
 * Represents a definition
 */
class Definition {
  /** @var string $headword */
  public $headword;
  /** @var string $definition */
  public $definition;

  public $isDefinitionOf =[];

  public static function serialize(Definition $definition): array
  {
    $data = [];
    $data['headword'] = $definition->headword;
    $data['definition'] = cleanString($definition->definition);
    $data['isDefinitionOf'] = $definition->isDefinitionOf;
    return $data;
  }
}

class BibliographicReference {
  public $reference;
  public $isReferenceOf = [];

  public static function serialize(BibliographicReference $reference): array
  {
    $data = [];
    $data['name'] = cleanString($reference->reference);
    $data['description'] = '';
    $data['url'] = '';
    $data['isExternalResourceOf'] = array_unique($reference->isReferenceOf);
    return $data;
  }
}

class Link {
  public $relation = "http://www.w3.org/2004/02/skos/core#broader";
  public $relationName = 'is subconcept of';
  public $target;
  public $source;

  public static function serialize(Link $link): array
  {
    $data = [];
    $data['relation'] = $link->relation;
    $data['relationName'] = $link->relationName;
    $data['target'] = $link->target;
    $data['source'] = $link->source;
    return $data;
  }
}

class LearningOutcome {
  public $label;
  public $isLearningOutcomeOf = [];

  public static function serialize(LearningOutcome $learningOutcome): array
  {
    $data = [];
    $data['label'] = utf8_encode(substr($learningOutcome->label, 0, 255));
    $data['definition'] = cleanString($learningOutcome->label);
    $data['isLearningOutcomeOf'] = $learningOutcome->isLearningOutcomeOf;
    return $data;
  }
}

class Keyword {
  public $id;
  public $name;
  public $isKeywordOf;

  public static function serialize(Keyword $keyword): array
  {
    $data = [];
    $data['id'] = $keyword->id;
    $data['name'] = cleanString($keyword->name);
    $data['isKeywordOf'] = $keyword->isKeywordOf;
    return $data;
  }
}

function bok_export_drush_command(): array
{
  return [
    'bok-export' => [
      'description' => 'Export BoK data.',
      'aliases' => ['boke'],
      'arguments' => [
        'path' => 'Path to store export file.'
      ],
      'options' => [
        'limit' => 'Limit export to number of records',
        's' => 'Strip HTML tags'
      ]
    ]
  ];
}

/**
 * Strip out HTML, trim, clean up newlines, etc.
 * @param string|null $string
 * @return string
 */
function cleanString(?string $string = null): string
{
  if (!$string) {
    return '';
  }
  $string = str_replace(['&lsquo;', '&rsquo;'], "'", $string);
  $string = str_replace(['&amp;'], "&", $string);
  $string = str_replace('&nbsp;', ' ', $string);
  $string = str_replace([PHP_EOL, "\r\t", "\r"], ' ', $string);
  $string = strip_tags($string);
  return addslashes($string);
}

/**
 * The main export logic
 *
 * @param $path
 * @return void
 */
function drush_bok_export($path = null) {
  $data = [
    'nodes' => [],
    'links' => [],
    'learning_outcomes' => [],
    'keywords' => []
  ];

  $ka = [];
  $keywords = [];

  $learningOutcomes = [];
  libxml_use_internal_errors(true);

  $stripTags = drush_get_option('s') ?? false;

  $nodeId = 0;
  $topicCount = 0;
  $topicNames = [];
  $bibliography = [];

  // Create a top-level node
  $node = new Node();
  $node->id = $nodeId;
  $node->title = 'UCGIS';
  $data['nodes'][] = Node::serialize($node, $stripTags);

  // The knowledge areas are a taxonomy with id 15 in the BoK site
  $knowledgeAreas = taxonomy_get_tree(15);
  // Load all the knowledge areas as nodes
  foreach ($knowledgeAreas as $knowledgeArea) {
    $ka[$knowledgeArea->tid] = $nodeId;

    $node = new Node();
    $node->title = $knowledgeArea->name;
    $node->name = 'https://gistbok.ucgis.org' . url('taxonomy/' . $knowledgeArea->tid);
    $data['nodes'][] = Node::serialize($node, $stripTags);

    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'topics')
      ->propertyCondition('status', NODE_PUBLISHED)
      ->fieldCondition('field_tag_1', 'tid', $knowledgeArea->tid);

    $limit = drush_get_option('limit') ?? null;
    if ($limit) {
      $query->range(1, $limit);
    }

    $result = $query->execute();

    $nids = array_keys($result['node']);
    $drupalNodes = entity_load('node', $nids);
    foreach ($drupalNodes as $topicNode) {
      $url = url('node/' . $topicNode->nid);
      $topicCode = $topicNode->field_code['und'][0]['value'];
      // AM1-2
      if (in_array($topicCode, $topicNames) || (strpos($url, '2016-quarter-02') !== false) || preg_match('/\w{2}\d+-\d+/', $topicCode)) {
        // skip earlier versions and previously seen topics
        continue;
      }
      $topicNames[] = $topicCode;
      $topicCount++;

      // Go to the next knowledge area's nodes
      if ($limit && $topicCount > $limit) {
        $topicCount = 0;
        break;
      }

      if ($topicNode->field_tag_2 && is_array($topicNode->field_tag_2['und'])) {
        foreach ($topicNode->field_tag_2['und'] as $keyword) {
          $result = taxonomy_term_load($keyword['tid']);
          $keywordId = (int)$keyword['tid'];
          if (!isset($keywords[$keywordId])) {
            $keywordObject = new Keyword();
            $keywordObject->id = $keywordId;
            $keywordObject->name = $result->name;
            $keywords[$keywordId] = $keywordObject;
          }
          $keywords[$keywordId]->isKeywordOf[] = $nodeId;
        }
      }

      // Additional Resources: field_additional_resources

      // Author and citation info: field_author_and_citation_info

      // field_bibliography

      // topic description: field_topic_contents

      // Content: body

      $nodeId++;
      // Parse nodes
      $node = new Node();
      $node->id = $nodeId;
      $node->drupalId = $topicNode->nid;
      $node->title = $topicNode->title;
      $node->code = $topicCode;
      $node->name = 'https://gistbok.ucgis.org' . $url;
      $node->definition = $topicNode->body && $topicNode->body['und'] ? strip_tags($topicNode->body['und'][0]['value']) : null;

      $node->content = $topicNode->field_topic_contents && $topicNode->field_topic_contents['und'] ? $topicNode->field_topic_contents['und'][0]['value'] : null;

      // Split up bibliography entries
      $biblioContent = $topicNode->field_bibliography['und'][0]['value'] ?? null;
      if (!empty($biblioContent)) {
        $biblioDoc = new DOMDocument();
        $biblioDoc->loadHtml($biblioContent);
        foreach($biblioDoc->getElementsByTagName('p') as $line) {
          $entry = trim(strip_tags($line->textContent));
          if (empty($entry)) {
            continue;
          }
          if (!isset($bibliography[md5($entry)])) {
            $biblioObject = new BibliographicReference();
            $biblioObject->reference = $entry;
            $bibliography[md5($entry)] = $biblioObject;
          }
          $bibliography[md5($entry)]->isReferenceOf[] = $nodeId;
        }
      }

      $data['nodes'][] = Node::serialize($node, $stripTags);

      // Parse links
      $link = new Link();
      $link->source = $node->id;
      if (isset($ka[$topicNode->field_tag_1['und'][0]['tid']])) {
        $link->target = $ka[$topicNode->field_tag_1['und'][0]['tid']];
        $data['links'][] = Link::serialize($link);
      }

      // Parse learning outcomes
      $outcomes = new DOMDocument();
      $outcomes->loadHTML($topicNode->field_learning_objectives['und'][0]['value']);
      foreach ($outcomes->getElementsByTagName('li') as $outcome) {
        $outcomeText = trim($outcome->textContent);
        $key = md5($outcomeText);
        if (!isset($learningOutcomes[$key])) {
          $learningOutcomes[$key] = new LearningOutcome();
          $learningOutcomes[$key]->label = $outcomeText;
        }
        $learningOutcomes[$key]->isLearningOutcomeOf[] = $nodeId;
      }
    }
    ++$nodeId;
  }

  print(count($data['nodes'])  . " nodes\n");

  foreach ($learningOutcomes as $learningOutcome) {
    $data['learning_outcomes'][] = LearningOutcome::serialize($learningOutcome);
  }
  print(count($data['learning_outcomes']) . " learning outcomes\n");

  foreach ($keywords as $keyword) {
    $data['keywords'][] = Keyword::serialize($keyword);
  }

  print(count($data['keywords'])  . " keywords\n");


  foreach ($bibliography as $item) {
    $resource = BibliographicReference::serialize($item);
    if (empty(trim($resource['name']))) {
      continue;
    }
    $data['external_resources'][] = $resource;
  }

  print(count($data['external_resources']) . " biblio\n");

  $jsonData = json_encode($data, JSON_UNESCAPED_SLASHES & JSON_INVALID_UTF8_IGNORE & JSON_UNESCAPED_UNICODE);
  if (!$jsonData) {
    print(json_last_error_msg());
  }

  file_put_contents($path, $jsonData, JSON_UNESCAPED_SLASHES & JSON_INVALID_UTF8_IGNORE);
}
