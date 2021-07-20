<?php

class Node {
  public $id;
  public $drupalId;
  public $topic;
  public $title;
  public $name;

  public static function serialize(Node $node): array
  {
    $data = [];

    if ($node->topic) {
      $data['label'] = "[{$node->topic}] {$node->title}";
    } else {
      $data['label'] = $node->title;
    }
    $data['nodeName'] = $node->name;
    $data['definition'] = $node->title;
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
  public $name;
  public $label;
  public $isLearningOutcomeOf = [];

  public static function serialize(LearningOutcome $learningOutcome): array
  {
    $data = [];
    $data['name'] = $learningOutcome->name ?? '';
    $data['label'] = substr($learningOutcome->label, 0, 25);
    $data['definition'] = $learningOutcome->label;
    $data['isLearningOutcomeOf'] = $learningOutcome->isLearningOutcomeOf;
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
      ]
    ]
  ];
}

function drush_bok_export($path = null) {
  $data = [
    'nodes' => [],
    'links' => [],
    'learning_outcomes' => []
  ];

  $knowledgeAreas = taxonomy_get_tree(15);
  $ka = [];

  $learningOutcomes = [];
  libxml_use_internal_errors(true);

  $nodeId = 0;
  $topicCount = 0;
  $topicNames = [];

  // Load all the knowledge areas as nodes
  foreach ($knowledgeAreas as $knowledgeArea) {
    $ka[$knowledgeArea->tid] = $nodeId;

    $node = new Node();
    $node->title = $knowledgeArea->name;
    $node->name = 'https://gistbok.ucgis.org' . url('taxonomy/' . $knowledgeArea->tid);
    $data['nodes'][] = Node::serialize($node);

    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'topics')
      ->propertyCondition('status', NODE_PUBLISHED)
      ->fieldCondition('field_tag_1', 'tid', $knowledgeArea->tid);
//      ->range(0, 10);

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
      if ($topicCount > 100) {
        $topicCount = 0;
        break;
      }

      $nodeId++;
      // Parse nodes
      $node = new Node();
      $node->id = $nodeId;
      $node->drupalId = $topicNode->nid;
      $node->title = $topicNode->title;
      $node->topic = $topicCode;
      $node->name = 'https://gistbok.ucgis.org' . $url;

      $data['nodes'][] = Node::serialize($node);

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
        $outcomeText = preg_replace('/[^A-Za-z0-9\- ]/', '', $outcomeText);
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

  foreach ($learningOutcomes as $learningOutcome) {
    $data['learning_outcomes'][] = LearningOutcome::serialize($learningOutcome);
  }


//  $query = new EntityFieldQuery();
//  $query->entityCondition('entity_type', 'node')
//    ->entityCondition('bundle', 'topics')
//    ->propertyCondition('status', NODE_PUBLISHED)
//    ->fieldCondition('field_tag_1', 'tid', $tids[1]);
//    ->range(0, 400);


  file_put_contents($path, json_encode($data), JSON_UNESCAPED_SLASHES & JSON_INVALID_UTF8_IGNORE);
}
